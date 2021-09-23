#pragma once

#include "core/logger.h"

#include <filesystem>
#include <fstream>
#include <string>

struct File
{
    static std::string
    LoadFile(const std::filesystem::path & path)
    {
        std::ifstream ifs;
        std::filesystem::path full_path = std::filesystem::absolute(path);
        ifs.open(full_path);
        if (!ifs.is_open())
        {
            Logger::Error<true>("FileLoader cannot open file : " + full_path.string());
        }
        Logger::Info("FileLoader loaded file : " + full_path.string());
        std::stringstream buffer;
        buffer << ifs.rdbuf();
        return buffer.str();
    }
};