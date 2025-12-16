#include "./file.hpp"
#include <vector>
#include <string>
#include <tuple>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstdlib>
#include <stdexcept>
#ifdef _WIN32
#include <shlobj.h>
#include <knownfolders.h>
#include <windows.h>
#endif

namespace Util::File
{
    namespace fs = std::filesystem;

    namespace
    {
        fs::path getHomeDirectory()
        {
#ifdef _WIN32
            if (const char *userProfile = std::getenv("USERPROFILE"))
            {
                if (*userProfile != '\0')
                {
                    return fs::path{userProfile};
                }
            }

            const char *homeDrive = std::getenv("HOMEDRIVE");
            const char *homePath = std::getenv("HOMEPATH");
            if (homeDrive && homePath)
            {
                return fs::path{std::string{homeDrive} + homePath};
            }

            if (const char *homeEnv = std::getenv("HOME"))
            {
                if (*homeEnv != '\0')
                {
                    return fs::path{homeEnv};
                }
            }
#else
            if (const char *homeEnv = std::getenv("HOME"))
            {
                if (*homeEnv != '\0')
                {
                    return fs::path{homeEnv};
                }
            }
#endif
            return {};
        }

        std::string stripQuotes(const std::string &text)
        {
            if (text.size() >= 2 && text.front() == '"' && text.back() == '"')
            {
                return text.substr(1, text.size() - 2);
            }
            return text;
        }

        fs::path expandUserDirTemplate(const std::string &value, const fs::path &home)
        {
            if (value.empty())
            {
                return {};
            }

            std::string result;
            result.reserve(value.size() + (home.empty() ? 0 : home.string().size()));

            for (std::size_t i = 0; i < value.size(); ++i)
            {
                const char ch = value[i];

                if (ch == '$' && value.compare(i, 5, "$HOME") == 0)
                {
                    if (!home.empty())
                    {
                        result += home.string();
                    }
                    i += 4;
                    continue;
                }

                if (ch == '~' && i == 0)
                {
                    if (!home.empty())
                    {
                        result += home.string();
                    }
                    continue;
                }

                if (ch == '\\' && i + 1 < value.size())
                {
                    ++i;
                    result.push_back(value[i]);
                    continue;
                }

                result.push_back(ch);
            }

            return fs::path{result};
        }

        fs::path parseXdgDownloadDir(const fs::path &home)
        {
            const fs::path configPath = home / ".config" / "user-dirs.dirs";
            std::ifstream in{configPath};
            if (!in)
            {
                return {};
            }

            std::string line;
            while (std::getline(in, line))
            {
                const auto commentPos = line.find('#');
                if (commentPos != std::string::npos)
                {
                    line.erase(commentPos);
                }

                const auto firstNotSpace = line.find_first_not_of(" \t");
                if (firstNotSpace == std::string::npos)
                {
                    continue;
                }

                constexpr std::string_view key = "XDG_DOWNLOAD_DIR=";
                if (line.compare(firstNotSpace, key.size(), key) != 0)
                {
                    continue;
                }

                std::string value = stripQuotes(line.substr(firstNotSpace + key.size()));
                fs::path resolved = expandUserDirTemplate(value, home);
                if (!resolved.empty())
                {
                    return resolved;
                }
            }

            return {};
        }

        fs::path getSystemDownloadsDirectory()
        {
#ifdef _WIN32
            if (PWSTR downloadsWide = nullptr;
                SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Downloads, KF_FLAG_DEFAULT, nullptr, &downloadsWide)))
            {
                fs::path resolved{std::wstring{downloadsWide}};
                CoTaskMemFree(downloadsWide);
                if (!resolved.empty())
                {
                    return resolved;
                }
            }
#endif

            const fs::path home = getHomeDirectory();

#if defined(__linux__) || defined(__unix__)
            if (!home.empty())
            {
                if (auto xdg = parseXdgDownloadDir(home); !xdg.empty())
                {
                    return xdg;
                }
            }
#endif

            if (!home.empty())
            {
                return home / "Downloads";
            }

            return {};
        }
    } // namespace

    std::string normalizeRelativePath(const std::string &path)
    {
        std::string sanitized = path;
        std::replace(sanitized.begin(), sanitized.end(), '\\', '/');

        fs::path normalized = fs::path{sanitized}.lexically_normal();

        if (normalized.has_root_path())
        {
            normalized = normalized.relative_path();
        }

        const std::string generic = normalized.generic_string();
        if (generic == ".")
        {
            return {};
        }

        return generic;
    }

    bool containsParentTraversal(const std::string &path)
    {
        for (const auto &part : fs::path{path}.lexically_normal())
        {
            if (part == "..")
            {
                return true;
            }
        }

        return false;
    }

    std::string urlEncode(std::string_view text)
    {
        std::string encoded;
        encoded.reserve(text.size() * 3 / 2);
        constexpr char hexDigits[] = "0123456789ABCDEF";
        for (unsigned char ch : text)
        {
            if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')
                || ch == '-' || ch == '_' || ch == '.' || ch == '~')
            {
                encoded.push_back(static_cast<char>(ch));
            }
            else if (ch == ' ')
            {
                encoded += "%20";
            }
            else
            {
                encoded.push_back('%');
                encoded.push_back(hexDigits[(ch >> 4) & 0x0F]);
                encoded.push_back(hexDigits[ch & 0x0F]);
            }
        }

        return encoded;
    }

    std::tuple<bool, std::string> sanitizeUploadFilename(const std::string &input)
    {
        std::string normalized = normalizeRelativePath(input);
        if (normalized.empty() || containsParentTraversal(normalized))
        {
            return {false, {}};
        }

        std::string sanitized = fs::path{normalized}.filename().string();
        if (sanitized.empty() || sanitized == "." || sanitized == "..")
        {
            return {false, {}};
        }

        for (char &ch : sanitized)
        {
            if (ch == '/' || ch == '\\')
            {
                ch = '_';
            }
        }

        return {true, sanitized};
    }

    std::tuple<bool, fs::path, std::string> chooseUploadDestination(const fs::path &uploadsDir, const std::string &sanitized)
    {
        fs::path base = uploadsDir / sanitized;
        fs::path destination = base;
        std::error_code ec;
        std::size_t suffix = 1;
        while (fs::exists(destination, ec))
        {
            if (ec)
            {
                return {false, {}, ec.message()};
            }

            const std::string stem = base.stem().string();
            const std::string extension = base.extension().string();
            destination = uploadsDir / (stem + "_" + std::to_string(suffix) + extension);
            ++suffix;
        }
        if (ec)
        {
            return {false, {}, ec.message()};
        }

        return {true, destination, {}};
    }

    bool isWithinBase(const fs::path &candidate, const fs::path &base)
    {
        std::error_code ec;
        auto relativePath = fs::relative(candidate, base, ec);
        if (ec)
        {
            return false;
        }

        for (const auto &part : relativePath)
        {
            if (part == "..")
            {
                return false;
            }
        }

        return true;
    }

    std::string buildHrefForPath(const std::string &relativePath)
    {
        if (relativePath.empty())
        {
            return "/";
        }

        std::string href{"/"};
        bool firstSegment = true;
        for (const auto &segment : fs::path{relativePath})
        {
            auto segmentStr = segment.string();
            if (segmentStr.empty())
            {
                continue;
            }

            if (!firstSegment)
            {
                href.push_back('/');
            }

            href += urlEncode(segmentStr);
            firstSegment = false;
        }

        if (firstSegment)
        {
            return "/";
        }

        return href;
    }

    std::string escapeForHtml(std::string_view text)
    {
        std::string escaped;
        escaped.reserve(text.size());
        for (char ch : text)
        {
            switch (ch)
            {
            case '&':
                escaped += "&amp;";
                break;
            case '<':
                escaped += "&lt;";
                break;
            case '>':
                escaped += "&gt;";
                break;
            case '"':
                escaped += "&quot;";
                break;
            case '\'':
                escaped += "&#39;";
                break;
            default:
                escaped.push_back(ch);
                break;
            }
        }

        return escaped;
    }

    fs::path getDefaultUploadsDirectory(const fs::path &baseDir)
    {
        if (auto downloads = getSystemDownloadsDirectory(); !downloads.empty())
        {
            return downloads / "accio";
        }

        return baseDir / "accio";
    }

    std::tuple<bool, fs::path, std::string> resolveUploadsDirectory(const fs::path &candidateInput)
    {
        if (candidateInput.empty())
        {
            return {false, {}, "empty path"};
        }

        fs::path candidate = candidateInput;
        if (candidate.is_relative())
        {
            candidate = fs::current_path() / candidate;
        }

        std::error_code createEc;
        fs::create_directories(candidate, createEc);
        if (createEc)
        {
            std::error_code existsEc;
            if (!fs::exists(candidate, existsEc))
            {
                return {false, {}, createEc.message()};
            }

            if (existsEc)
            {
                return {false, {}, existsEc.message()};
            }
        }

        std::error_code statusEc;
        auto status = fs::status(candidate, statusEc);
        if (statusEc)
        {
            return {false, {}, statusEc.message()};
        }

        if (!fs::is_directory(status))
        {
            return {false, {}, "path exists and is not a directory"};
        }

        std::error_code canonicalEc;
        const auto canonical = fs::weakly_canonical(candidate, canonicalEc);
        if (canonicalEc)
        {
            return {false, {}, canonicalEc.message()};
        }

        return {true, canonical, {}};
    }

    std::string formatFileSize(std::uintmax_t bytes)
    {
        constexpr std::uintmax_t KB = 1024;
        constexpr std::uintmax_t MB = KB * 1024;
        constexpr std::uintmax_t GB = MB * 1024;
        constexpr std::uintmax_t TB = GB * 1024;

        auto formatWithUnit = [](double value, const char *unit) {
            std::ostringstream oss;
            if (value < 10.0)
            {
                oss << std::fixed << std::setprecision(2);
            }
            else if (value < 100.0)
            {
                oss << std::fixed << std::setprecision(1);
            }
            else
            {
                oss << std::fixed << std::setprecision(0);
            }
            oss << value << ' ' << unit;
            return oss.str();
        };

        if (bytes >= TB)
        {
            return formatWithUnit(static_cast<double>(bytes) / static_cast<double>(TB), "TB");
        }
        if (bytes >= GB)
        {
            return formatWithUnit(static_cast<double>(bytes) / static_cast<double>(GB), "GB");
        }
        if (bytes >= MB)
        {
            return formatWithUnit(static_cast<double>(bytes) / static_cast<double>(MB), "MB");
        }
        if (bytes >= KB)
        {
            return formatWithUnit(static_cast<double>(bytes) / static_cast<double>(KB), "KB");
        }
        return std::to_string(bytes) + " B";
    }
} // namespace Util::File
