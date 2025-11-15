#pragma once

#include <string>

class Core
{
public:
    Core() = default;
    ~Core() = default;

    void start(const std::string &path = {});

private:
};
