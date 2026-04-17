#include "textboardEntry.hpp"
#include <format>
#include <utility>

namespace Model
{

    TextboardEntry::TextboardEntry(std::string ip, std::string text)
        : ip(std::move(ip)),
          text(std::move(text)),
          time(std::chrono::system_clock::now())
    {
    }

    std::string TextboardEntry::formatTime() const
    {
        const auto localTime = std::chrono::zoned_time{std::chrono::current_zone(), std::chrono::floor<std::chrono::seconds>(time)};
        return std::format("{:%Y-%m-%d %H:%M:%S}", localTime);
    }

    std::string TextboardEntry::toJson() const
    {
        auto escape = [](const std::string &text) {
            std::string result;
            result.reserve(text.size());
            for (char character : text)
            {
                switch (character)
                {
                case '"':
                    result += "\\\"";
                    break;
                case '\\':
                    result += "\\\\";
                    break;
                case '\n':
                    result += "\\n";
                    break;
                case '\r':
                    result += "\\r";
                    break;
                case '\t':
                    result += "\\t";
                    break;
                default:
                    result += character;
                }
            }
            return result;
        };

        return R"({"ip":")" + escape(ip) + R"(","text":")" + escape(text) + R"(","time":")" + formatTime() + R"("})";
    }
} // namespace Model
