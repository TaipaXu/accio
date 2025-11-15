#pragma once

#include <string>

class Core
{
public:
    Core() = default;
    ~Core() = default;

    void start(const std::string &path, const std::string &uploadsPath, const std::string &host, unsigned short port) const;

private:
};
