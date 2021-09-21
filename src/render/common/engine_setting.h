#pragma once

#include "shared.h"

struct EngineSetting
{
    static const uint32_t MaxNumGeometries               = 1000;
    static const uint32_t MaxNumBindlessTextures         = 1000;
    static const uint32_t MaxNumStandardMaterials        = 1000;
    static const uint32_t MaxNumVertices                 = 400000;
    static const uint32_t MaxNumIndices                  = 400000;
    static const uint32_t MaxNumGeometryOffsetTableEntry = 100;
    static const uint32_t MaxNumGeometryTableEntry       = 100;
    static const uint32_t MaxNumInstances                = 100;
};