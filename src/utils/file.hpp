#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <tuple>
#include <filesystem>
#include <cstdint>

namespace Util::File
{
    namespace fs = std::filesystem;

    std::string normalizeRelativePath(const std::string &path);

    bool containsParentTraversal(const std::string &path);

    std::string urlEncode(std::string_view text);

    std::tuple<bool, std::string> sanitizeUploadFilename(const std::string &input);

    std::tuple<bool, fs::path, std::string> chooseUploadDestination(const fs::path &uploadsDir, const std::string &sanitized);

    bool isWithinBase(const fs::path &candidate, const fs::path &base);

    std::string buildHrefForPath(const std::string &relativePath);

    std::string escapeForHtml(std::string_view text);

    fs::path getDefaultUploadsDirectory(const fs::path &baseDir);

    std::tuple<bool, fs::path, std::string> resolveUploadsDirectory(const fs::path &candidateInput);

    std::string formatFileSize(std::uintmax_t bytes);

    bool hasAbsolutePaths(const std::vector<std::string> &items);
} // namespace Util::File
