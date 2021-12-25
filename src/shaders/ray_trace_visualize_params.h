#pragma once

#ifndef RT_VISUALIZE_CB_PARAMS
    #define RT_VISUALIZE_CB_PARAMS

    #include "cpp_compatible.h"
    #include "shared/bindless_table.h"
    #include "shared/camera_params.h"
    #include "shared/compact_vertex.h"
    #include "shared/standard_material.h"

enum class RaytraceVisualizeModeEnum : uint32_t
{
    ModeInstanceId,
    ModeBaseInstanceId,
    ModeGeometryId,
    ModeTriangleId,
    ModeBaryCentricCoords,
    ModePosition,
    ModeGeometryNormal,
    ModeShadingNormal,
    ModeTextureCoords,
    ModeDepth,
    ModeDiffuseReflectance,
    ModeSpecularReflectance,
    ModeRoughness
};

struct RaytraceVisualizeCbParams
{
    float4x4                  m_camera_inv_view;
    float4x4                  m_camera_inv_proj;
    RaytraceVisualizeModeEnum m_mode;
};

REGISTER_WRAP_BEGIN(RayTraceVisualizeRegisters)
// Set 0
ConstantBuffer<RaytraceVisualizeCbParams> REGISTER(0, u_cbparams, b, 0);
RWTexture2D<float4>                       REGISTER(0, u_output, u, 0);

// Set 1
SamplerState                              REGISTER(1, u_sampler, s, 0);
RaytracingAccelerationStructure           REGISTER(1, u_scene_bvh, t, 0);
StructuredBuffer<BaseInstanceTableEntry>  REGISTER(1, u_base_instance_table, t, 1);
StructuredBuffer<GeometryTableEntry>      REGISTER(1, u_geometry_table, t, 2);
StructuredBuffer<uint16_t>                REGISTER(1, u_indices, t, 3);
StructuredBuffer<float3>                  REGISTER(1, u_positions, t, 4);
StructuredBuffer<CompactVertex>           REGISTER(1, u_compact_vertices, t, 5);
StructuredBuffer<StandardMaterial>        REGISTER(1, u_materials, t, 6);
Texture2D<float4>                         REGISTER_ARRAY(1, u_textures, 100, t, 7);
REGISTER_WRAP_END

#endif