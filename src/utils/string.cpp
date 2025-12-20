#include "string.hpp"
#include <algorithm>
#include <random>
#include <cctype>

namespace Util::String
{
    std::string toLowerCopy(std::string_view text)
    {
        std::string lower{text.begin(), text.end()};
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        return lower;
    }

    std::string generateRandomString(std::size_t length)
    {
        static constexpr char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        static constexpr std::size_t charsetSize = sizeof(charset) - 1U;

        std::random_device rd;
        std::mt19937 engine(rd());
        std::uniform_int_distribution<std::size_t> dist(0U, charsetSize - 1U);

        std::string result;
        result.reserve(length);
        for (std::size_t i = 0; i < length; ++i)
        {
            result.push_back(charset[dist(engine)]);
        }
        return result;
    }
} // namespace Util::String
