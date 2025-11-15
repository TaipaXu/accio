#include "./core.hpp"
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <system_error>
#include <filesystem>
#include <utility>
#include <drogon/drogon.h>
#include <drogon/MultiPart.h>
#include <drogon/utils/Utilities.h>
#include "indexHtml.hpp"

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
} // namespace

void Core::start(const std::string &path, const std::string &host, unsigned short port) const
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

    const auto uploadsDir = baseDir / "uploads";
    std::error_code createDirEc;
    fs::create_directories(uploadsDir, createDirEc);
    if (createDirEc)
    {
        throw std::runtime_error("failed to prepare uploads directory: " + uploadsDir.string());
    }

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
