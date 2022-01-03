#pragma once

#include "cpp_compatible.h"
#include "shared/bindless_table.h"
#include "shared/camera_params.h"
#include "shared/compact_vertex.h"
#include "shared/standard_material.h"

struct GbufferGenerationRayTraceCbParams
{
    float4x4 m_camera_inv_view;
    float4x4 m_camera_inv_proj;
    float    m_scene_tmin;
    float    m_scene_tmax;
};

struct RAY_PAYLOAD GbufferGenerationRayTracePayload
{
    int2 m_pixel_pos;
};

struct GbufferGenerationRayTraceAttributes
{
    float2 uv;
};

REGISTER_WRAP_BEGIN(GbufferGenerationRayTraceRegisters)
// Set 0
ConstantBuffer<GbufferGenerationRayTraceCbParams> REGISTER(0, u_cbparams, b, 0);
RWTexture2D<float>                                REGISTER(0, u_gbuffer_depth, u, 1);
RWTexture2D<float3>                               REGISTER(0, u_gbuffer_shading_normal, u, 2);
RWTexture2D<float3>                               REGISTER(0, u_gbuffer_diffuse_reflectance, u, 3);
RWTexture2D<float3>                               REGISTER(0, u_gbuffer_specular_reflectance, u, 4);
RWTexture2D<float>                                REGISTER(0, u_gbuffer_roughness, u, 5);

// Set 1
SamplerState                             REGISTER(1, u_sampler, s, 0);
RaytracingAccelerationStructure          REGISTER(1, u_scene_bvh, t, 0);
StructuredBuffer<BaseInstanceTableEntry> REGISTER(1, u_base_instance_table, t, 1);
StructuredBuffer<GeometryTableEntry>     REGISTER(1, u_geometry_table, t, 2);
StructuredBuffer<uint16_t>               REGISTER(1, u_indices, t, 3);
StructuredBuffer<CompactVertex>          REGISTER(1, u_compact_vertices, t, 4);
StructuredBuffer<StandardMaterial>       REGISTER(1, u_materials, t, 5);
Texture2D<float4>                        REGISTER_ARRAY(1, u_textures, 100, t, 6);
REGISTER_WRAP_END
