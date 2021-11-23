#pragma once

#include <filesystem>

struct EngineSetting
{
    static const uint32_t MaxNumBindlessTextures         = 100;
    static const uint32_t MaxNumStandardMaterials        = 1000;
    static const uint32_t MaxNumVertices                 = 10000000;
    static const uint32_t MaxNumIndices                  = 30000000;
    static const uint32_t MaxNumGeometryOffsetTableEntry = 14000;
    static const uint32_t MaxNumGeometryTableEntry       = 32000;

    inline static std::filesystem::path &
    ShaderCachePath()
    {
        static std::filesystem::path result = "shadercache/";
        return result;
    }
};