#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>
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
               bool passwordEnabled,
               const std::vector<std::string> &allowedExtensions = {},
               const std::vector<std::string> &deniedExtensions = {},
               const std::vector<std::string> &allowedFiles = {},
               const std::vector<std::string> &deniedFiles = {});
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
    static std::unordered_set<std::string> normalizeExtensions(const std::vector<std::string> &extensions);
    static void resolveDeniedPaths(const std::vector<std::string> &items,
                                   const std::filesystem::path &baseDir,
                                   std::unordered_set<std::string> &outFiles,
                                   std::vector<std::filesystem::path> &outDirs);
    static void resolveAllowedPaths(const std::vector<std::string> &items,
                                    const std::filesystem::path &baseDir,
                                    std::unordered_set<std::string> &outFiles,
                                    std::vector<std::filesystem::path> &outDirs,
                                    std::unordered_set<std::string> &outAncestors);
    static bool isInList(const std::filesystem::path &canonicalPath,
                         const std::unordered_set<std::string> &fileSet,
                         const std::vector<std::filesystem::path> &dirList);
    static bool isEntryAccessible(const std::filesystem::path &canonicalPath,
                                  bool isDirectory,
                                  const std::unordered_set<std::string> &allowedFiles,
                                  const std::vector<std::filesystem::path> &allowedDirs,
                                  const std::unordered_set<std::string> &allowedAncestors,
                                  const std::unordered_set<std::string> &deniedFiles,
                                  const std::vector<std::filesystem::path> &deniedDirs,
                                  const std::unordered_set<std::string> &normalizedAllowedExts,
                                  const std::unordered_set<std::string> &normalizedDeniedExts,
                                  bool hasAllowedExt,
                                  bool hasDeniedExt,
                                  bool hasAllowedFiles);

private:
    std::atomic_bool authRequired{false};
    mutable std::mutex authMutex;
    std::unordered_set<std::string> authorizedIps;
    std::mutex serverMutex;
    std::shared_ptr<httplib::Server> server;
};
