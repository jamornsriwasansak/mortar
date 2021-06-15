#ifndef PBVERTEX_H
#define PBVERTEX_H

#include "sharedcommon.h"

struct HlslVertexIn
{
    float3 m_position;
    float  m_texcoord_x;
    float3 m_normal;
    float  m_texcoord_y;
};

struct HlslTransform
{
    float4x4 m_world_from_model;
};

#ifdef __cplusplus
// make sure shader transform can be splitted into dwords and aligned by 16
static_assert(sizeof(HlslTransform) % 16 == 0);
#endif

#endif // PBVERTEX_H
