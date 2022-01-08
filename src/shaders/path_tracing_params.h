#pragma once

#include "cpp_compatible.h"
#include "shared/bindless_table.h"
#include "shared/camera_params.h"
#include "shared/compact_vertex.h"
#include "shared/standard_material.h"

struct PathTracingCbParams
{
    float4x4 m_camera_inv_view;
    float4x4 m_camera_inv_proj;
    uint32_t m_radiance_miss_shader_index;
    uint32_t m_shadow_miss_shader_index;
};

struct RAY_PAYLOAD PathTracingPayload
{
    int   m_is_first_bounce;
    int   m_is_last_bounce;
    int   m_miss;
    float  m_hit_dist;
    float2 m_rnd2;
    float3 m_next_dir;
    float  m_t;
};

struct RAY_PAYLOAD PathTracingShadowRayPayload
{
    bool m_hit;
};

struct PathTracingAttributes
{
    float2 uv;
};

REGISTER_WRAP_BEGIN(PathTracingRegisters)
// Set 0
ConstantBuffer<PathTracingCbParams> REGISTER(0, u_params, b, 0);
RWTexture2D<float3>                 REGISTER(0, u_demodulated_diffuse_gi, u, 0);
RWTexture2D<float3>                 REGISTER(0, u_demodulated_specular_gi, u, 1);
RWTexture2D<float>                  REGISTER(0, u_gbuffer_depth, u, 2);
RWTexture2D<float3>                 REGISTER(0, u_gbuffer_shading_normal, u, 3);
RWTexture2D<float3>                 REGISTER(0, u_gbuffer_diffuse_reflectance, u, 4);
RWTexture2D<float3>                 REGISTER(0, u_gbuffer_specular_reflectance, u, 5);
RWTexture2D<float>                  REGISTER(0, u_gbuffer_roughness, u, 6);

// Set 1
SamplerState                             REGISTER(1, u_sampler, s, 0);
StructuredBuffer<BaseInstanceTableEntry> REGISTER(1, u_base_instance_table, t, 1);
StructuredBuffer<GeometryTableEntry>     REGISTER(1, u_geometry_table, t, 2);
StructuredBuffer<uint16_t>               REGISTER(1, u_indices, t, 3);
StructuredBuffer<CompactVertex>          REGISTER(1, u_compact_vertices, t, 4);
StructuredBuffer<StandardMaterial>       REGISTER(1, u_materials, t, 5);
Texture2D<float4>                        REGISTER_ARRAY(1, u_textures, 100, t, 6);
RaytracingAccelerationStructure          REGISTER(1, u_scene_bvh, t, 0);
REGISTER_WRAP_END