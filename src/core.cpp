#include "./core.hpp"
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <system_error>
#include <filesystem>
#include <fstream>
#include <utility>
#include <cstdlib>
#include <drogon/drogon.h>
#include <drogon/MultiPart.h>
#include <drogon/utils/Utilities.h>
#include "indexHtml.hpp"
#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#include <knownfolders.h>
#endif

namespace fs = std::filesystem;

namespace
{
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

    std::string buildContentDispositionHeader(const std::string &filename)
    {
        std::string sanitized = filename;
        for (char &ch : sanitized)
        {
            if (ch == '"' || ch == '\\')
            {
                ch = '_';
            }
        }

        auto encoded = drogon::utils::urlEncode(filename);
        std::string header = "attachment; filename=\"" + sanitized + "\"";
        header += "; filename*=UTF-8''" + encoded;
        return header;
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

            href += drogon::utils::urlEncode(segmentStr);
            firstSegment = false;
        }

        if (firstSegment)
        {
            return "/";
        }

        return href;
    }

    std::string extractRelativePath(const drogon::HttpRequestPtr &req)
    {
        std::string requestPath = req->getParameter("path");
        if (requestPath.empty())
        {
            requestPath = req->path();
        }

        requestPath = drogon::utils::urlDecode(requestPath);
        return normalizeRelativePath(requestPath);
    }

    drogon::HttpResponsePtr makePlainTextResponse(drogon::HttpStatusCode status, std::string_view body)
    {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(status);
        resp->setContentTypeCode(drogon::CT_TEXT_PLAIN);
        resp->setBody(std::string{body});
        return resp;
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

    fs::path systemDownloadsDirectory()
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

    fs::path defaultUploadsDirectory()
    {
        if (auto downloads = systemDownloadsDirectory(); !downloads.empty())
        {
            return downloads / "accio";
        }

        return fs::current_path() / "Downloads" / "accio";
    }

    bool resolveUploadsDirectory(const fs::path &candidateInput, fs::path &resolved, std::string &error)
    {
        if (candidateInput.empty())
        {
            error = "empty path";
            return false;
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
                error = createEc.message();
                return false;
            }

            if (existsEc)
            {
                error = existsEc.message();
                return false;
            }
        }

        std::error_code statusEc;
        auto status = fs::status(candidate, statusEc);
        if (statusEc)
        {
            error = statusEc.message();
            return false;
        }

        if (!fs::is_directory(status))
        {
            error = "path exists and is not a directory";
            return false;
        }

        std::error_code canonicalEc;
        const auto canonical = fs::weakly_canonical(candidate, canonicalEc);
        if (canonicalEc)
        {
            error = canonicalEc.message();
            return false;
        }

        resolved = canonical;
        return true;
    }
} // namespace

void Core::start(const std::string &path, const std::string &uploadsPath, const std::string &host, unsigned short port) const
{
    auto &app = drogon::app();
    app.addListener(host, port);

    std::error_code ec;
    fs::path baseCandidate = path.empty() ? fs::current_path() : fs::path{path};
    if (baseCandidate.is_relative())
    {
        baseCandidate = fs::current_path() / baseCandidate;
    }

    const auto baseDir = fs::weakly_canonical(baseCandidate, ec);
    if (ec || !fs::exists(baseDir) || !fs::is_directory(baseDir))
    {
        throw std::runtime_error("invalid base directory: " + baseCandidate.string());
    }

    app.registerHandlerViaRegex(
        "/.*",
        [baseDir](const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
            std::string relativePath = extractRelativePath(req);

            if (containsParentTraversal(relativePath))
            {
                callback(makePlainTextResponse(drogon::k403Forbidden, "Forbidden path"));
                return;
            }

            fs::path target = baseDir;
            if (!relativePath.empty())
            {
                target /= fs::path{relativePath};
            }

            std::error_code ec;
            const auto canonicalTarget = fs::weakly_canonical(target, ec);
            if (ec || !fs::exists(canonicalTarget) || !isWithinBase(canonicalTarget, baseDir))
            {
                callback(makePlainTextResponse(drogon::k404NotFound, "Entry not found"));
                return;
            }

            if (fs::is_regular_file(canonicalTarget))
            {
                auto resp = drogon::HttpResponse::newFileResponse(canonicalTarget.string());
                resp->addHeader("Content-Disposition", buildContentDispositionHeader(canonicalTarget.filename().string()));
                callback(resp);
                return;
            }

            if (!fs::is_directory(canonicalTarget))
            {
                callback(makePlainTextResponse(drogon::k404NotFound, "Entry not found"));
                return;
            }

            std::vector<std::pair<std::string, bool>> directoryEntries;
            for (const auto &entry : fs::directory_iterator{canonicalTarget})
            {
                directoryEntries.emplace_back(entry.path().filename().string(), entry.is_directory());
            }

            std::sort(directoryEntries.begin(), directoryEntries.end(), [](const auto &lhs, const auto &rhs) {
                if (lhs.second != rhs.second)
                {
                    return lhs.second > rhs.second;
                }
                return lhs.first < rhs.first;
            });

            std::string filesHtml;
            filesHtml.reserve(128 + directoryEntries.size() * 64);
            filesHtml += "<ul>\n";

            if (!relativePath.empty())
            {
                const auto parentPath = fs::path{relativePath}.parent_path().generic_string();
                const std::string href = buildHrefForPath(parentPath);
                filesHtml += "<li><a href=\"" + href + "\">↩ ../</a></li>\n";
            }

            for (const auto &[filename, isDirectory] : directoryEntries)
            {
                const std::string childPath = relativePath.empty() ? filename : relativePath + "/" + filename;
                const std::string href = buildHrefForPath(childPath);
                const std::string displayName = isDirectory ? "📁 " + filename + "/" : filename;

                filesHtml += "<li><a href=\"" + href + "\">" + escapeForHtml(displayName) + "</a></li>\n";
            }

            filesHtml += "</ul>\n";

            std::string html = resources::index_html;
            const std::string placeholder = "{{files}}";
            if (auto pos = html.find(placeholder); pos != std::string::npos)
            {
                html.replace(pos, placeholder.size(), filesHtml);
            }

            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setBody(std::move(html));
            resp->setContentTypeCode(drogon::CT_TEXT_HTML);
            callback(resp);
        },
        {drogon::Get, drogon::Head});

    const fs::path uploadsCandidate = uploadsPath.empty() ? defaultUploadsDirectory() : fs::path{uploadsPath};
    fs::path uploadsDir;
    std::string candidateError;
    if (!resolveUploadsDirectory(uploadsCandidate, uploadsDir, candidateError))
    {
        const fs::path fallbackUploads = baseDir / "accio";
        std::string fallbackError;
        if (!resolveUploadsDirectory(fallbackUploads, uploadsDir, fallbackError))
        {
            throw std::runtime_error(
                "failed to prepare uploads directory. primary '" + uploadsCandidate.string() + "' (" + candidateError + ")"
                + "; fallback '" + fallbackUploads.string() + "' (" + fallbackError + ")");
        }
    }
    std::cout << "Uploads directory: " << uploadsDir.string() << std::endl;

    app.registerHandler(
        "/upload",
        [uploadsDir](const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
            drogon::MultiPartParser parser;
            if (parser.parse(req) != 0)
            {
                callback(makePlainTextResponse(drogon::k400BadRequest, "Invalid multipart payload"));
                return;
            }

            const auto &files = parser.getFiles();
            if (files.empty())
            {
                callback(makePlainTextResponse(drogon::k400BadRequest, "No files provided"));
                return;
            }

            std::vector<std::string> sanitizedNames;
            sanitizedNames.reserve(files.size());
            for (const auto &file : files)
            {
                std::string normalizedName = normalizeRelativePath(file.getFileName());
                if (normalizedName.empty() || containsParentTraversal(normalizedName))
                {
                    callback(makePlainTextResponse(drogon::k400BadRequest, "Invalid file name"));
                    return;
                }

                std::string sanitized = fs::path{normalizedName}.filename().string();
                if (sanitized.empty() || sanitized == "." || sanitized == "..")
                {
                    callback(makePlainTextResponse(drogon::k400BadRequest, "Invalid file name"));
                    return;
                }

                for (char &ch : sanitized)
                {
                    if (ch == '/' || ch == '\\')
                    {
                        ch = '_';
                    }
                }

                sanitizedNames.push_back(std::move(sanitized));
            }

            std::vector<std::string> savedNames;
            savedNames.reserve(files.size());
            for (std::size_t i = 0; i < files.size(); ++i)
            {
                const fs::path baseDestination = uploadsDir / sanitizedNames[i];
                fs::path destination = baseDestination;
                std::size_t suffix = 1;
                while (fs::exists(destination))
                {
                    const std::string stem = baseDestination.stem().string();
                    const std::string extension = baseDestination.extension().string();
                    destination = uploadsDir / (stem + "_" + std::to_string(suffix) + extension);
                    ++suffix;
                }

                if (files[i].saveAs(destination.string()) != 0)
                {
                    callback(makePlainTextResponse(drogon::k500InternalServerError, "Failed to save file"));
                    return;
                }

                savedNames.push_back(destination.filename().string());
            }

            std::string responseBody = "Uploaded files:\n";
            for (const auto &name : savedNames)
            {
                responseBody += name;
                responseBody.push_back('\n');
            }

            callback(makePlainTextResponse(drogon::k200OK, responseBody));
        },
        {drogon::Post});

    app.run();
}
