#pragma once

#include <chrono>
#include <string>

namespace Model
{

    struct TextboardEntry
    {
        std::string ip;
        std::string text;
        std::chrono::system_clock::time_point time;

        TextboardEntry(std::string ip, std::string text);

        std::string formatTime() const;
        std::string toJson() const;
    };
} // namespace Model
