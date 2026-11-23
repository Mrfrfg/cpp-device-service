#include <array>
#include <cerrno>
#include <cstdio>
#include <iostream>
#include <string>
#include <string_view>
#include "unique_fd.h"
#include "network_config.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

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

int main(int argc, char* argv[])
{
    std::uint16_t port = default_port;
    int command_start = 1;

    if (argc >= 2 &&
        std::string_view(argv[1]) == "--port") {
        if (argc < 4 ||
           !parse_port(argv[2], port)) {
           std::cerr
               << "Usage: device_client "
               << "[--port PORT] "
               << "<command> [command...]\n";
            return 1;
        }

        command_start = 3;
    }

    if (command_start >= argc) {
        std::cerr
            << "Usage: device_client "
            << "[--port PORT] "
            << "<command> [command...]\n";
         return 1;
    }

    UniqueFd client_fd{
    socket(AF_INET, SOCK_STREAM, 0)
    };

    if (!client_fd)  {
        perror("socket");
        return 1;
    }

    sockaddr_in server_address{};
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);

    if (inet_pton(
            AF_INET,
            "127.0.0.1",
            &server_address.sin_addr) != 1) {
        std::cerr << "Invalid server address\n";
        return 1;
    }

    if (connect(
            client_fd.get(),
            reinterpret_cast<sockaddr*>(&server_address),
            sizeof(server_address)) == -1) {
        perror("connect");
        return 1;
    }

    std::string request;

   for (int index = command_start;
     index < argc;
     ++index) {
    request += argv[index];
    request += '\n';
}

    request += "QUIT\n";

    if (!send_all(client_fd.get(), request)) {
        perror("send");
        return 1;
    }

    if (shutdown(client_fd.get(), SHUT_WR) == -1) {
        perror("shutdown");
        return 1;
    }

    std::array<char, 1024> buffer{};

    while (true) {
        const ssize_t received = recv(
            client_fd.get(),
            buffer.data(),
            buffer.size(),
            0);

        if (received == 0) {
            break;
        }

        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }

            perror("recv");
            return 1;
        }

        std::cout.write(
            buffer.data(),
            static_cast<std::streamsize>(received));
    }

    return 0;
}
