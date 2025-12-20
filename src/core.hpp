#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <filesystem>
#include <unordered_set>

namespace httplib
{
    class Server;
    class Response;
} // namespace httplib

class Core
{
private:
    enum class Color
    {
        Green,
        Red
    };

public:
    Core() = default;
    ~Core() = default;

    void start(const std::string &path,
               const std::string &uploadsPath,
               const std::string &host,
               unsigned short port,
               bool uploadsEnabled,
               const std::string &password,
               bool passwordEnabled);
    void stop();

private:
    static void logStartupInfo(const std::string &host,
                               unsigned short port,
                               const std::string &uploadsDir,
                               bool uploadsEnabled,
                               const std::string &password,
                               bool passwordEnabled);
    static void printLine(bool colorEnabled, const std::string &label, const std::string &value, Color color = Color::Green);
    bool isAuthorized(const std::string &ip) const;
    void authorizeIp(const std::string &ip);
    static inline void setPlainTextResponse(httplib::Response &response, int status, std::string_view body);
    static std::string buildContentDispositionHeader(const std::string &filename);
    static bool streamFileResponse(httplib::Response &response, const std::filesystem::path &filePath);
    static bool caseInsensitiveLess(const std::string &lhs, const std::string &rhs);

private:
    std::atomic_bool authRequired{false};
    mutable std::mutex authMutex;
    std::unordered_set<std::string> authorizedIps;
    std::mutex serverMutex;
    std::shared_ptr<httplib::Server> server;
};
