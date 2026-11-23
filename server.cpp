#include "device_store.h"

#include <array>
#include <cerrno>
#include <condition_variable>
#include <csignal>
#include <cstdio>
#include <iostream>
#include <mutex>
#include <queue>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#include "unique_fd.h"
#include <utility>
#include "network_config.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

std::mutex log_mutex;
volatile std::sig_atomic_t stop_requested = 0;

void request_stop(int)
{
    stop_requested = 1;
}

bool send_all(int fd, std::string_view data)
{
    std::size_t sent_total = 0;

    while (sent_total < data.size()) {
        const ssize_t sent = send(
            fd,
            data.data() + sent_total,
            data.size() - sent_total,
            MSG_NOSIGNAL);

        if (sent < 0 && errno == EINTR) {
            continue;
        }

        if (sent <= 0) {
            return false;
        }

        sent_total += static_cast<std::size_t>(sent);
    }

    return true;
}

void reject_busy_client(UniqueFd client_fd)
{
    timeval timeout{};
    timeout.tv_usec = 200000;

    setsockopt(
        client_fd.get(),
        SOL_SOCKET,
        SO_RCVTIMEO,
        &timeout,
        sizeof(timeout));

    send_all(client_fd.get(), "ERROR server busy\n");

    // 发出 FIN，但保留读取方向，避免带未读数据直接 close 产生 RST。
    shutdown(client_fd.get(), SHUT_WR);

    std::array<char, 1024> discard{};
    std::size_t discarded = 0;

    while (discarded < 4096) {
        const ssize_t received = recv(
            client_fd.get(),
            discard.data(),
            discard.size(),
            0);

        if (received > 0) {
            discarded +=
                static_cast<std::size_t>(received);
            continue;
        }

        if (received < 0 && errno == EINTR) {
            continue;
        }

        break;
    }

}

void handle_client(UniqueFd client_fd, DeviceStore& store)
{
    const int fd = client_fd.get();
    {
        std::lock_guard<std::mutex> lock(log_mutex);
        std::cout << "Client connected: fd="
                  << fd << '\n';
    }
    
    timeval receive_timeout{};
    receive_timeout.tv_sec = 1;
    receive_timeout.tv_usec = 0;

    if (setsockopt(
            client_fd.get(),
            SOL_SOCKET,
            SO_RCVTIMEO,
            &receive_timeout,
            sizeof(receive_timeout)) == -1) {
        perror("setsockopt SO_RCVTIMEO");
        return;
    }

    std::array<char, 1024> chunk{};
    std::string pending;
    bool client_active = true;

    while (client_active && !stop_requested) {
        const ssize_t received = recv(
            client_fd.get(),
            chunk.data(),
            chunk.size(),
            0);

        if (received == 0) {
            break;
        }

        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }

            if (errno == EAGAIN ||
                errno == EWOULDBLOCK) {
                if (stop_requested) {
                    break;
                }

                continue;
            }

            perror("recv");
            break;
        }

        pending.append(
            chunk.data(),
            static_cast<std::size_t>(received));

        std::size_t newline = 0;

        while ((newline = pending.find('\n')) !=
               std::string::npos) {
            if (newline > 4096) {
                send_all(
                    client_fd.get(),
                    "ERROR command too long\n");

                client_active = false;
                break;
            }

            const std::string line =
                pending.substr(0, newline);

            pending.erase(0, newline + 1);

            const CommandResult result =
                store.execute(line);

            if (!send_all(
                    client_fd.get(),
                    result.response)) {
                client_active = false;
                break;
            }

            if (result.close_connection) {
                client_active = false;
                break;
            }
        }

        if (pending.size() > 4096) {
            send_all(
                client_fd.get(),
                "ERROR command too long\n");

            break;
        }
    }


    {
        std::lock_guard<std::mutex> lock(log_mutex);
        std::cout << "Client disconnected: fd="
                  << fd << '\n';
    }
}

int main(int argc, char* argv[])
{
    std::uint16_t port = default_port;

    if (argc > 2 ||
        (argc == 2 && !parse_port(argv[1], port))) {
        std::cerr
            << "Usage: device_server [port]\n";
        return 1;
    }
    
    struct sigaction action {};
    action.sa_handler = request_stop;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    if (sigaction(SIGINT, &action, nullptr) == -1) {
        perror("sigaction");
        return 1;
    }

    UniqueFd server_fd {
        socket(AF_INET, SOCK_STREAM, 0)};

    if (!server_fd) {
        perror("socket");
        return 1;
    }

    const int reuse = 1;

    if (setsockopt(
            server_fd.get(),
            SOL_SOCKET,
            SO_REUSEADDR,
            &reuse,
            sizeof(reuse)) == -1) {
        perror("setsockopt SO_REUSEADDR");
        return 1;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr =
        htonl(INADDR_LOOPBACK);

    if (bind(
            server_fd.get(),
            reinterpret_cast<sockaddr*>(&address),
            sizeof(address)) == -1) {
        perror("bind");
        return 1;
    }

    if (listen(server_fd.get(), 16) == -1) {
        perror("listen");
        return 1;
    }

    std::cout
        << "Listening on 127.0.0.1:" << port << '\n';;

    DeviceStore store;

    std::queue<UniqueFd> client_queue;
    constexpr std::size_t max_pending_clients = 4;

    std::mutex queue_mutex;
    std::condition_variable queue_changed;
    bool stopping = false;

    constexpr int worker_count = 2;
    std::vector<std::thread> workers;
    workers.reserve(worker_count);

    for (int index = 0;
         index < worker_count;
         ++index) {
        workers.emplace_back([&] {
            while (true) {
                UniqueFd client_fd;

                {
                    std::unique_lock<std::mutex> lock(
                        queue_mutex);

                    queue_changed.wait(lock, [&] {
                        return stopping ||
                               !client_queue.empty();
                    });

                    if (stopping &&
                        client_queue.empty()) {
                        return;
                    }

                    client_fd =
                        std::move(client_queue.front());

                    client_queue.pop();
                }

                handle_client( std::move(client_fd), store);
            }
        });
    }

    while (!stop_requested) {
       UniqueFd client_fd{
       accept(server_fd.get(), nullptr, nullptr)};

        if (!client_fd) {
            if (errno == EINTR) {
                if (stop_requested) {
                    break;
                }
 
                continue;
            }

            perror("accept");
            break;
        }

        bool queued = false;

{
    std::lock_guard<std::mutex> lock(
        queue_mutex);

    if (client_queue.size() <
        max_pending_clients) {
        client_queue.push(std::move(client_fd));
        queued = true;
    }
}

if (queued) {
    queue_changed.notify_one();
} else {
    reject_busy_client(std::move(client_fd));

    std::lock_guard<std::mutex> lock(
        log_mutex);

    std::cout
        << "Rejected client: queue full\n";
}
    }

    server_fd.reset();

    {
        std::lock_guard<std::mutex> lock(
            queue_mutex);

        stopping = true;

        while (!client_queue.empty()) {
            client_queue.pop();
        }
    }

    queue_changed.notify_all();

    for (std::thread& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    std::cout << "Server stopped cleanly\n";
    return 0;
}
