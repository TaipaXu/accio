#include "textboardEntry.hpp"
#include <version>
#include <utility>
#if defined(__cpp_lib_format) && defined(__cpp_lib_chrono) && __cpp_lib_chrono >= 201907L
#include <format>
#else
#include <ctime>
#include <cstdio>
#endif

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
#if defined(__cpp_lib_format) && defined(__cpp_lib_chrono) && __cpp_lib_chrono >= 201907L
        const auto localTime = std::chrono::zoned_time{std::chrono::current_zone(), std::chrono::floor<std::chrono::seconds>(time)};
        return std::format("{:%Y-%m-%d %H:%M:%S}", localTime);
#else
        const time_t tt = std::chrono::system_clock::to_time_t(time);
        std::tm local{};
        localtime_r(&tt, &local);
        char buf[20];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &local);
        return std::string{buf};
#endif
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
