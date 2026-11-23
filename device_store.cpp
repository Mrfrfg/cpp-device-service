#include "device_store.h"

#include <cmath>
#include <sstream>
#include <string>

namespace
{

bool has_extra_argument(std::istringstream& input)
{
    std::string extra;
    return static_cast<bool>(input >> extra);
}

}

CommandResult DeviceStore::execute(
    const std::string& line)
{
    std::istringstream input(line);
    std::string command;
    input >> command;

    if (command == "SET") {
        std::string device_id;
        double temperature = 0.0;

        if (!(input >> device_id >> temperature) ||
            has_extra_argument(input)) {
            return {
                "ERROR usage: SET "
                "<device-id> <temperature>\n",
                false
            };
        }

        if (!std::isfinite(temperature)) {
            return {
                "ERROR invalid temperature\n",
                false
            };
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            data_[device_id] = temperature;
        }

        return {"OK\n", false};
    }

    if (command == "GET") {
        std::string device_id;

        if (!(input >> device_id) ||
            has_extra_argument(input)) {
            return {
                "ERROR usage: GET <device-id>\n",
                false
            };
        }

        std::lock_guard<std::mutex> lock(mutex_);
        const auto position = data_.find(device_id);

        if (position == data_.end()) {
            return {"NOT_FOUND\n", false};
        }

        std::ostringstream output;
        output << "VALUE "
               << position->first
               << ' '
               << position->second
               << '\n';

        return {output.str(), false};
    }

    if (command == "QUIT") {
        if (has_extra_argument(input)) {
            return {
                "ERROR usage: QUIT\n",
                false
            };
        }

        return {"BYE\n", true};
    }

    return {"ERROR unknown command\n", false};
}
