#pragma once

#include "cpp_compatible.h"

struct PathTracingCbParams
{
    float4x4 m_camera_inv_view;
    float4x4 m_camera_inv_proj;
};

struct RAY_PAYLOAD PathTracingPayload
{
    float4 m_hit;
};

struct PathTracingAttributes
{
    float2 uv;
};

REGISTER_WRAP_BEGIN(PathTracingRegisters)
// Set 0
ConstantBuffer<PathTracingCbParams> REGISTER(0, u_params, b, 0);
RWTexture2D<float3>                 REGISTER(0, u_diffuse_direct_light_result, u, 0);
RWTexture2D<float>                  REGISTER(0, u_gbuffer_depth, u, 1);
RWTexture2D<float3>                 REGISTER(0, u_gbuffer_shading_normal, u, 2);

// Set 1
// SamplerState                    REGISTER(1, u_sampler, s, 0);
RaytracingAccelerationStructure REGISTER(1, u_scene_bvh, t, 0);
REGISTER_WRAP_END