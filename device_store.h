#pragma once

#include <mutex>
#include <string>
#include <unordered_map>

struct CommandResult
{
    std::string response;
    bool close_connection;
};

class DeviceStore
{
public:
    CommandResult execute(const std::string& line);

private:
    std::unordered_map<std::string, double> data_;
    std::mutex mutex_;
};
