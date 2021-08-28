#include "../../common/shared.h"
#include "../../common/camera_params.h"

struct Payload
{
    float3 m_color;
    bool m_miss;
};

struct Attributes
{
    float2 uv;
};

struct CameraInfo
{
    float4x4 m_inv_view;
    float4x4 m_inv_proj;
};

RaytracingAccelerationStructure u_scene_bvh REGISTER(t0, space0);
ConstantBuffer<CameraInfo> u_camera         REGISTER(b0, space0);
RWTexture2D<float4> u_output                REGISTER(u0, space0);

SHADER_TYPE("raygeneration")
void
RayGen()
{
    uint2 pixel = DispatchRaysIndex().xy;
    uint2 resolution = DispatchRaysDimensions().xy;
    uint pixel_index = pixel.y * resolution.x + pixel.x;

    const float2 uv = (float2(pixel) + float2(0.5f, 0.5f)) / float2(resolution);
    const float2 ndc = uv * 2.0f - 1.0f;

    const float3 origin = mul(u_camera.m_inv_view, float4(0.0f, 0.0f, 0.0f, 1.0f)).xyz;
    const float3 lookat = mul(u_camera.m_inv_proj, float4(ndc.x, ndc.y, 1.0f, 1.0f)).xyz;
    const float3 direction = mul(u_camera.m_inv_view, float4(normalize(lookat), 0)).xyz;

    Payload payload;
    payload.m_miss = true;
    payload.m_color = float3(0.0f, 0.0f, 0.0f);

    // Setup the ray
    RayDesc ray;
    ray.Origin = origin;
    ray.Direction = direction;
    ray.TMin = 0.1f;
    ray.TMax = 1000.f;

    // Trace the ray
    TraceRay(u_scene_bvh, RAY_FLAG_FORCE_OPAQUE, 0xFF, 0, 0, 0, ray, payload);

    u_output[pixel] = float4(payload.m_color, 1.0f);
}

SHADER_TYPE("closesthit")void ClosestHit(INOUT(Payload) payload, const Attributes attrib)
{
    payload.m_miss = false;
    payload.m_color = float3(1.0f, 1.0f, 0.0f);
}

SHADER_TYPE("miss")
void Miss(INOUT(Payload) payload)
{
    payload.m_miss = true;
}
