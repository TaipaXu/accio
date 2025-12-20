#pragma once

#include <string>
#include <string_view>
#include <cstddef>

namespace Util::String
{
    std::string toLowerCopy(std::string_view text);
    std::string generateRandomString(std::size_t length);
} // namespace Util::String
