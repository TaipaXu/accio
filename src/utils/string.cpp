#include "string.hpp"

#include <algorithm>
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
} // namespace Util::String
