#ifndef PBVERTEX_H
#define PBVERTEX_H

#include "shared.h"

struct CompactVertex
{
    float3 m_position;
    float  m_texcoord_x;
    float3 m_normal;
    float  m_texcoord_y;
};

#endif // PBVERTEX_H
