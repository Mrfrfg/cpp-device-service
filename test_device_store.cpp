#include <thread>
#include <vector>
#include "device_store.h"

#include <iostream>
#include <string>

int failures = 0;

void expect_equal(
    const std::string& actual,
    const std::string& expected,
    const std::string& test_name)
{
    if (actual == expected) {
        std::cout << "PASS: " << test_name << '\n';
        return;
    }

    std::cerr << "FAIL: " << test_name << '\n'
              << "  expected: " << expected
              << "  actual:   " << actual;
    ++failures;
}

void expect_true(bool value, const std::string& test_name)
{
    if (value) {
        std::cout << "PASS: " << test_name << '\n';
        return;
    }

    std::cerr << "FAIL: " << test_name << '\n';
    ++failures;
}

int main()
{
    DeviceStore store;

    expect_equal(
        store.execute("GET sensor-01").response,
        "NOT_FOUND\n",
        "unknown device");

    expect_equal(
        store.execute("SET sensor-01 21.5").response,
        "OK\n",
        "store reading");

    expect_equal(
        store.execute("GET sensor-01").response,
        "VALUE sensor-01 21.5\n",
        "retrieve reading");

    expect_equal(
        store.execute("SET").response,
        "ERROR usage: SET <device-id> <temperature>\n",
        "reject incomplete SET");

    expect_equal(
        store.execute("DELETE sensor-01").response,
        "ERROR unknown command\n",
        "reject unknown command");

    const CommandResult quit = store.execute("QUIT");

    expect_equal(quit.response, "BYE\n", "QUIT response");
    expect_true(quit.close_connection, "QUIT closes connection");
    
        {
        DeviceStore concurrent_store;

        constexpr int thread_count = 8;
        constexpr int updates_per_thread = 200;

        std::vector<std::thread> workers;
        workers.reserve(thread_count);

        for (int thread_index = 0;
             thread_index < thread_count;
             ++thread_index) {
            workers.emplace_back(
                [&concurrent_store, thread_index] {
                    const std::string device_id =
                        "sensor-" + std::to_string(thread_index);

                    for (int value = 0;
                         value < updates_per_thread;
                         ++value) {
                        concurrent_store.execute(
                            "SET " + device_id + " "
                            + std::to_string(value));
                    }
                });
        }

        for (std::thread& worker : workers) {
            worker.join();
        }

        for (int thread_index = 0;
             thread_index < thread_count;
             ++thread_index) {
            const std::string device_id =
                "sensor-" + std::to_string(thread_index);

            expect_equal(
                concurrent_store.execute(
                    "GET " + device_id).response,
                "VALUE " + device_id + " 199\n",
                "concurrent update " + device_id);
        }
    }

    expect_equal(
    store.execute(
        "SET sensor-01 99 extra").response,
    "ERROR usage: SET <device-id> <temperature>\n",
    "reject extra SET argument");

expect_equal(
    store.execute(
        "SET sensor-01 99oops").response,
    "ERROR usage: SET <device-id> <temperature>\n",
    "reject malformed temperature");

expect_equal(
    store.execute(
        "GET sensor-01 extra").response,
    "ERROR usage: GET <device-id>\n",
    "reject extra GET argument");

const CommandResult invalid_quit =
    store.execute("QUIT extra");

expect_equal(
    invalid_quit.response,
    "ERROR usage: QUIT\n",
    "reject extra QUIT argument");

expect_true(
    !invalid_quit.close_connection,
    "invalid QUIT keeps connection open");

expect_equal(
    store.execute("GET sensor-01").response,
    "VALUE sensor-01 21.5\n",
    "invalid SET does not overwrite value");
    
    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return 1;
    }

    std::cout << "All tests passed\n";
    return 0;
}

