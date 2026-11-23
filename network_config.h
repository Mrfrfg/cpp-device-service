#pragma once

#include <charconv>
#include <cstdint>
#include <string_view>
#include <system_error>

inline constexpr std::uint16_t default_port = 9000;

inline bool parse_port(
    std::string_view text,
    std::uint16_t& port)
{
    unsigned int value = 0;

    const char* begin = text.data();
    const char* end = begin + text.size();

    const auto result =
        std::from_chars(begin, end, value);

    if (result.ec != std::errc{} ||
        result.ptr != end ||
        value == 0 ||
        value > 65535) {
        return false;
    }

    port = static_cast<std::uint16_t>(value);
    return true;
}
