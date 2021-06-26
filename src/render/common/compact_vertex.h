#ifndef COMPACT_VERTEX_H
#define COMPACT_VERTEX_H

#include "shared.h"

struct CompactVertex
{
    float3 m_position;
    float  m_texcoord_x;
    float3 m_snormal;
    float  m_texcoord_y;
};

#endif // COMPACT_VERTEX_H
