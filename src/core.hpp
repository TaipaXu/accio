#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <filesystem>

namespace httplib
{
    class Server;
    class Response;
} // namespace httplib

class Core
{
public:
    Core() = default;
    ~Core() = default;

    void start(const std::string &path, const std::string &uploadsPath, const std::string &host, unsigned short port);
    void stop();

private:
    static void logStartupInfo(const std::string &host, unsigned short port, const std::string &uploadsDir);
    static void printLine(bool colorEnabled, const std::string &label, const std::string &value);
    static inline void setPlainTextResponse(httplib::Response &response, int status, std::string_view body);
    static std::string buildContentDispositionHeader(const std::string &filename);
    static bool streamFileResponse(httplib::Response &response, const std::filesystem::path &filePath);

    std::mutex serverMutex;
    std::shared_ptr<httplib::Server> server;
};
