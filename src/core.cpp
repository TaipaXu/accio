#include "./core.hpp"
#include <string>
#include <string_view>
#include <vector>
#include <unordered_set>
#include <tuple>
#include <utility>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <limits>
#include <memory>
#include <mutex>
#include <algorithm>
#include <type_traits>
#include <stdexcept>
#include <system_error>
#include <httplib.h>
#include "utils/file.hpp"
#include "utils/network.hpp"
#include "indexHtml.hpp"
#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace
{
    // Detect httplib version at compile time.
    // CPPHTTPLIB_VERSION_NUM was added in httplib 0.18.0 (2024).
    // Old versions (< 0.18) only have CPPHTTPLIB_VERSION string.
#ifdef CPPHTTPLIB_VERSION_NUM
    // New httplib has StatusCode enum and FormData type
    constexpr int HTTP_STATUS_OK = httplib::StatusCode::OK_200;
    constexpr int HTTP_STATUS_BAD_REQUEST = httplib::StatusCode::BadRequest_400;
    constexpr int HTTP_STATUS_FORBIDDEN = httplib::StatusCode::Forbidden_403;
    constexpr int HTTP_STATUS_NOT_FOUND = httplib::StatusCode::NotFound_404;
    constexpr int HTTP_STATUS_INTERNAL_SERVER_ERROR = httplib::StatusCode::InternalServerError_500;
    using UploadPartType = httplib::FormData;
#else
    // Old httplib doesn't have StatusCode enum; uses MultipartFormData
    constexpr int HTTP_STATUS_OK = 200;
    constexpr int HTTP_STATUS_BAD_REQUEST = 400;
    constexpr int HTTP_STATUS_FORBIDDEN = 403;
    constexpr int HTTP_STATUS_NOT_FOUND = 404;
    constexpr int HTTP_STATUS_INTERNAL_SERVER_ERROR = 500;
    using UploadPartType = httplib::MultipartFormData;
#endif
} // namespace

void Core::start(const std::string &path, const std::string &uploadsPath, const std::string &host, unsigned short port)
{
    constexpr std::size_t maxRequestBytes = 50ULL * 1024ULL * 1024ULL * 1024ULL; // 50GB

    auto httpServer = std::make_shared<httplib::Server>();
    httpServer->set_payload_max_length(maxRequestBytes);

    fs::path baseCandidate = path.empty() ? fs::current_path() : fs::path(path);
    if (baseCandidate.is_relative())
    {
        baseCandidate = fs::current_path() / baseCandidate;
    }

    std::error_code ec;
    const auto baseDir = fs::weakly_canonical(baseCandidate, ec);
    if (ec || !fs::exists(baseDir) || !fs::is_directory(baseDir))
    {
        throw std::runtime_error("invalid base directory: " + baseCandidate.string());
    }

    auto handleEntryRequest = [baseDir](const httplib::Request &request, httplib::Response &response) {
        const std::string relativePath = Util::File::normalizeRelativePath(request.path);
        fs::path target = baseDir;
        if (!relativePath.empty())
        {
            target /= fs::path{relativePath};
        }

        std::error_code ec;
        const auto canonicalTarget = fs::weakly_canonical(target, ec);
        if (ec || !fs::exists(canonicalTarget) || !Util::File::isWithinBase(canonicalTarget, baseDir))
        {
            setPlainTextResponse(response, HTTP_STATUS_NOT_FOUND, "Entry not found");
            return;
        }

        if (fs::is_regular_file(canonicalTarget))
        {
            if (!Core::streamFileResponse(response, canonicalTarget))
            {
                setPlainTextResponse(response, HTTP_STATUS_INTERNAL_SERVER_ERROR, "Failed to read file");
                return;
            }

            response.set_header("Content-Disposition", Core::buildContentDispositionHeader(canonicalTarget.filename().string()));
            return;
        }

        if (!fs::is_directory(canonicalTarget))
        {
            setPlainTextResponse(response, HTTP_STATUS_NOT_FOUND, "Entry not found");
            return;
        }

        struct Entry
        {
            std::string name;
            bool isDirectory;
            std::uintmax_t fileSize;
        };

        std::vector<Entry> entries;
        for (const auto &entry : fs::directory_iterator{canonicalTarget})
        {
            std::uintmax_t fileSize = 0;
            if (entry.is_regular_file())
            {
                std::error_code sizeEc;
                fileSize = entry.file_size(sizeEc);
                if (sizeEc)
                {
                    fileSize = 0;
                }
            }
            entries.push_back(Entry{entry.path().filename().string(), entry.is_directory(), fileSize});
        }

        std::sort(entries.begin(), entries.end(), [](const Entry &lhs, const Entry &rhs) {
            if (lhs.isDirectory != rhs.isDirectory)
            {
                return lhs.isDirectory > rhs.isDirectory;
            }
            return Core::caseInsensitiveLess(lhs.name, rhs.name);
        });

        std::string filesHtml;
        filesHtml += "<ul>\n";

        if (!relativePath.empty())
        {
            const auto parentPath = fs::path(relativePath).parent_path().generic_string();
            const std::string href = Util::File::buildHrefForPath(parentPath);
            filesHtml += "<li><a href=\"" + href + "\">↩ ../</a></li>\n";
        }

        for (const auto &[filename, isDirectory, fileSize] : entries)
        {
            const std::string childPath = relativePath.empty() ? filename : relativePath + "/" + filename;
            const std::string href = Util::File::buildHrefForPath(childPath);
            const std::string linkText = isDirectory ? "📁 " + filename + "/" : filename;
            std::string line = "<li><a href=\"" + href + "\">" + Util::File::escapeForHtml(linkText) + "</a>";
            if (!isDirectory)
            {
                line += " <span style=\"margin-left:10px;color:#888;\">[" + Util::File::formatFileSize(fileSize) + "]</span>";
            }
            line += "</li>\n";
            filesHtml += line;
        }

        filesHtml += "</ul>\n";

        std::string html = resources::index_html;
        const std::string placeholder = "{{files}}";
        if (auto pos = html.find(placeholder); pos != std::string::npos)
        {
            html.replace(pos, placeholder.size(), filesHtml);
        }

        response.set_content(std::move(html), "text/html");
    };

    httpServer->Get(R"(/.*)", handleEntryRequest);
    httpServer->set_pre_routing_handler([handleEntryRequest](const httplib::Request &request, httplib::Response &response) {
        if (request.method != "HEAD")
        {
            return httplib::Server::HandlerResponse::Unhandled;
        }

        handleEntryRequest(request, response);
        response.body.clear();
        return httplib::Server::HandlerResponse::Handled;
    });

    const bool userProvidedUploads = !uploadsPath.empty();
    const fs::path fallbackUploads = baseDir / "accio";
    const fs::path primaryUploads =
        userProvidedUploads ? fs::path{uploadsPath} : Util::File::getDefaultUploadsDirectory(baseDir);

    fs::path uploadsDir;
    const auto primaryResult = Util::File::resolveUploadsDirectory(primaryUploads);
    const bool primaryOk = std::get<0>(primaryResult);
    const fs::path primaryResolved = std::get<1>(primaryResult);
    const std::string primaryError = std::get<2>(primaryResult);

    if (!primaryOk)
    {
        const bool needFallback = userProvidedUploads || primaryUploads != fallbackUploads;
        if (!needFallback)
        {
            throw std::runtime_error(
                "failed to prepare uploads directory '" + primaryUploads.string() + "' (" + primaryError + ")");
        }

        const auto fallbackResult = Util::File::resolveUploadsDirectory(fallbackUploads);
        const bool fallbackOk = std::get<0>(fallbackResult);
        fs::path fallbackResolved = std::get<1>(fallbackResult);
        const std::string fallbackError = std::get<2>(fallbackResult);

        if (!fallbackOk)
        {
            throw std::runtime_error(
                "failed to prepare uploads directory. primary '" + primaryUploads.string() + "' (" + primaryError + ")"
                + "; fallback '" + fallbackUploads.string() + "' (" + fallbackError + ")");
        }

        uploadsDir = std::move(fallbackResolved);
    }
    else
    {
        uploadsDir = std::move(primaryResolved);
    }
    const std::string uploadsDirStr = uploadsDir.string();

    httpServer->Post(
        "/upload",
        [uploadsDir](const httplib::Request &request, httplib::Response &response, const httplib::ContentReader &content_reader) {
            if (!request.is_multipart_form_data())
            {
                setPlainTextResponse(response, HTTP_STATUS_BAD_REQUEST, "Invalid multipart payload");
                return;
            }

            enum class UploadError
            {
                None,
                BadRequest,
                Internal
            };

            UploadError error = UploadError::None;
            std::string errorMessage;

            std::ofstream currentFile;
            bool currentIsFile = false;
            bool hasFiles = false;
            std::vector<std::string> savedNames;

            auto fail = [&](UploadError type, std::string message) {
                if (error == UploadError::None)
                {
                    error = type;
                    errorMessage = std::move(message);
                }
                return false;
            };

            auto closeCurrent = [&]() {
                if (currentFile.is_open())
                {
                    currentFile.close();
                }
                currentIsFile = false;
            };

            bool ok = content_reader(
                [&](const UploadPartType &file) {
                    closeCurrent();
                    const std::string fileName = file.filename;
                    if (fileName.empty())
                    {
                        return true;
                    }

                    const auto [nameOk, sanitizedName] = Util::File::sanitizeUploadFilename(fileName);
                    if (!nameOk)
                    {
                        return fail(UploadError::BadRequest, "Invalid file name");
                    }

                    auto [destinationOk, destination, destinationError] =
                        Util::File::chooseUploadDestination(uploadsDir, sanitizedName);
                    if (!destinationOk)
                    {
                        return fail(UploadError::Internal, destinationError.empty() ? "Failed to save file" : destinationError);
                    }

                    currentFile.open(destination, std::ios::binary);
                    if (!currentFile)
                    {
                        return fail(UploadError::Internal, "Failed to save file");
                    }

                    currentIsFile = true;
                    hasFiles = true;
                    savedNames.push_back(destination.filename().string());
                    return true;
                },
                [&](const char *data, size_t dataLength) {
                    if (!currentIsFile)
                    {
                        return true;
                    }

                    currentFile.write(data, static_cast<std::streamsize>(dataLength));
                    if (!currentFile)
                    {
                        return fail(UploadError::Internal, "Failed to save file");
                    }
                    return true;
                });

            closeCurrent();

            if (!ok || error != UploadError::None)
            {
                auto status = error == UploadError::BadRequest ? HTTP_STATUS_BAD_REQUEST : HTTP_STATUS_INTERNAL_SERVER_ERROR;
                if (errorMessage.empty())
                {
                    errorMessage = "Upload failed";
                }
                setPlainTextResponse(response, status, errorMessage);
                return;
            }

            if (!hasFiles)
            {
                setPlainTextResponse(response, HTTP_STATUS_BAD_REQUEST, "No files provided");
                return;
            }

            std::string responseBody = "Uploaded files:\n";
            for (const auto &name : savedNames)
            {
                responseBody += name;
                responseBody.push_back('\n');
            }

            setPlainTextResponse(response, HTTP_STATUS_OK, responseBody);
        });

    const std::string listenerHost = host.empty() ? std::string{"0.0.0.0"} : host;
    unsigned short boundPort = port;
    bool boundOk = false;
    if (port == 0)
    {
        const auto dynamicPort = httpServer->bind_to_any_port(listenerHost.c_str());
        boundOk = dynamicPort > 0;
        boundPort = boundOk ? static_cast<unsigned short>(dynamicPort) : 0U;
    }
    else
    {
        boundOk = httpServer->bind_to_port(listenerHost.c_str(), static_cast<int>(port));
    }

    if (!boundOk)
    {
        throw std::runtime_error("failed to bind to " + listenerHost + ":" + std::to_string(port));
    }

    {
        std::lock_guard<std::mutex> guard(serverMutex);
        this->server = httpServer;
    }

    logStartupInfo(listenerHost, boundPort, uploadsDirStr);

    httpServer->listen_after_bind();

    {
        std::lock_guard<std::mutex> guard(serverMutex);
        this->server.reset();
    }
}

void Core::stop()
{
    std::shared_ptr<httplib::Server> runningServer;
    {
        std::lock_guard<std::mutex> guard(serverMutex);
        runningServer = this->server;
    }

    if (runningServer)
    {
        runningServer->stop();
    }
}

void Core::logStartupInfo(const std::string &host, unsigned short port, const std::string &uploadsDir)
{
    const std::string listenerHost = host.empty() ? std::string{"0.0.0.0"} : host;
    const auto formatUrl = [port](const std::string &address) {
        const std::string target = address.empty() ? std::string{"0.0.0.0"} : address;
        const bool requiresBrackets = target.find(':') != std::string::npos && target.front() != '[';
        if (requiresBrackets)
        {
            return "http://[" + target + "]:" + std::to_string(port);
        }
        return "http://" + target + ":" + std::to_string(port);
    };

    const std::string webAddress = formatUrl(listenerHost);

    bool colorEnabled = true;
#ifdef _WIN32
    static const bool vtEnabled = []() {
        const HANDLE stdOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (stdOut == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        DWORD mode = 0;
        if (!GetConsoleMode(stdOut, &mode))
        {
            return false;
        }

        if (mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING)
        {
            return true;
        }

        const DWORD updatedMode = mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        return SetConsoleMode(stdOut, updatedMode) != 0;
    }();
    colorEnabled = vtEnabled;
#endif

    const bool bindsAll = listenerHost == "0.0.0.0" || listenerHost == "::";
    const bool listensIpv6 = listenerHost.find(':') != std::string::npos && listenerHost != "0.0.0.0";
    std::vector<std::pair<std::string, std::string>> additionalEndpoints;
    std::unordered_set<std::string> seenUrls;
    seenUrls.insert(webAddress);

    if (bindsAll)
    {
        const auto addEndpoint = [&](const std::string &label, const std::string &address) {
            const std::string url = formatUrl(address);
            if (seenUrls.insert(url).second)
            {
                additionalEndpoints.emplace_back(label, url);
            }
        };

        addEndpoint("Local:", "localhost");
        if (listenerHost == "::")
        {
            addEndpoint("Local:", "::1");
        }

        for (const auto &[addr, family] : Util::Network::collectNetworkAddresses())
        {
            if (!listensIpv6 && family != AF_INET)
            {
                continue;
            }
            if (listensIpv6 && family != AF_INET6)
            {
                continue;
            }
            addEndpoint("Network:", addr);
        }
    }

    std::cout << "Accio started successfully!" << std::endl;
    printLine(colorEnabled, "Listening:", webAddress);
    for (const auto &[label, url] : additionalEndpoints)
    {
        printLine(colorEnabled, label, url);
    }
    printLine(colorEnabled, "Uploads:", uploadsDir);
    if (colorEnabled)
    {
        std::cout << "Use \033[31mControl\033[0m+\033[31mC\033[0m to stop the server safely." << std::endl;
    }
    else
    {
        std::cout << "Use Control+C to stop the server safely." << std::endl;
    }
    std::cout << std::flush;
}

void Core::printLine(bool colorEnabled, const std::string &label, const std::string &value)
{
    constexpr std::size_t labelWidth = 10U;
    const std::string padding = label.size() < labelWidth ? std::string(labelWidth - label.size(), ' ') : "";

    if (colorEnabled)
    {
        std::cout << "\033[34m" << label << "\033[0m";
    }
    else
    {
        std::cout << label;
    }

    if (!padding.empty())
    {
        std::cout << padding;
    }
    else
    {
        std::cout << ' ';
    }

    if (colorEnabled)
    {
        std::cout << "\033[32m" << value << "\033[0m" << std::endl;
    }
    else
    {
        std::cout << value << std::endl;
    }
}

void Core::setPlainTextResponse(httplib::Response &response, int status, std::string_view body)
{
    response.status = status;
    response.set_content(std::string{body}, "text/plain");
}

std::string Core::buildContentDispositionHeader(const std::string &filename)
{
    std::string sanitized = filename;
    for (char &ch : sanitized)
    {
        if (ch == '"' || ch == '\\')
        {
            ch = '_';
        }
    }

    const std::string encoded = Util::File::urlEncode(filename);
    std::string header = "attachment; filename=\"" + sanitized + "\"";
    header += "; filename*=UTF-8''" + encoded;
    return header;
}

bool Core::streamFileResponse(httplib::Response &response, const fs::path &filePath)
{
    std::error_code ec;
    const auto fileSize = fs::file_size(filePath, ec);
    if (ec)
    {
        return false;
    }

    if (fileSize > static_cast<uintmax_t>(std::numeric_limits<std::size_t>::max()))
    {
        return false;
    }

    auto fileStream = std::make_shared<std::ifstream>(filePath, std::ios::binary);
    if (!fileStream->is_open())
    {
        return false;
    }

    const std::size_t contentLength = static_cast<std::size_t>(fileSize);
    response.set_content_provider(
        contentLength,
        "application/octet-stream",
        [fileStream](std::size_t offset, std::size_t length, httplib::DataSink &sink) {
            if (length == 0)
            {
                return true;
            }

            if (!fileStream->good())
            {
                fileStream->clear();
            }

            fileStream->seekg(static_cast<std::streamoff>(offset), std::ios::beg);
            if (!fileStream->good())
            {
                return false;
            }

            static constexpr std::size_t chunkSize = 64U * 1024U;
            std::vector<char> buffer(chunkSize);
            std::size_t remaining = length;
            while (remaining > 0)
            {
                const std::size_t toRead = std::min(remaining, buffer.size());
                fileStream->read(buffer.data(), static_cast<std::streamsize>(toRead));
                const std::streamsize readBytes = fileStream->gcount();
                if (readBytes <= 0)
                {
                    return false;
                }

                if (!sink.write(buffer.data(), static_cast<std::size_t>(readBytes)))
                {
                    return false;
                }

                remaining -= static_cast<std::size_t>(readBytes);
            }

            return true;
        },
        [fileStream](bool) {
            if (fileStream->is_open())
            {
                fileStream->close();
            }
        });

    return true;
}

bool Core::caseInsensitiveLess(const std::string &lhs, const std::string &rhs)
{
    const auto toLowerCopy = [](const std::string &text) {
        std::string lower = text;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        return lower;
    };

    const std::string lhsLower = toLowerCopy(lhs);
    const std::string rhsLower = toLowerCopy(rhs);
    if (lhsLower == rhsLower)
    {
        return lhs < rhs;
    }
    return lhsLower < rhsLower;
}
