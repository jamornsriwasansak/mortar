#include "raytrace_visualize_params.h"
#include "shared/camera_params.h"
#include "shared/color_conversion.h"
#include "shared/standard_material.h"

struct Payload
{
    half3 m_color;
    bool m_miss;
};

struct Attributes
{
    float2 uv;
};

RaytracingAccelerationStructure u_scene_bvh : register(t0, space0);
ConstantBuffer<RaytraceVisualizeCbParams> u_cbparams : register(b0, space0);
RWTexture2D<float4> u_output : register(u0, space0);

struct StandardMaterialsContainer
{
    StandardMaterial m_materials[100];
};

SamplerState u_sampler : register(s0, space1);
Texture2D<float4> u_textures[100] : register(t0, space1);
ConstantBuffer<StandardMaterialsContainer> u_smc : register(b0, space1);
/*
StructuredBuffer<uint16_t>                 u_index_buffer     REGISTER(t1, space1);
StructuredBuffer<float3>                   u_vertex_buffer    REGISTER(t2, space1);
StructuredBuffer<CompactVertex>            u_compactv_buffer  REGISTER(t3, space1);
*/

[shader("raygeneration")]
void
RayGen()
{
    uint2 pixel      = DispatchRaysIndex().xy;
    uint2 resolution = DispatchRaysDimensions().xy;
    uint pixel_index = pixel.y * resolution.x + pixel.x;

    const half2 uv  = (float2(pixel) + float2(0.5h, 0.5h)) / float2(resolution);
    const half2 ndc = uv * 2.0h - 1.0h;

    const half3 origin    = mul(u_cbparams.m_camera_inv_view, float4(0.0f, 0.0f, 0.0f, 1.0f)).xyz;
    const half3 lookat    = mul(u_cbparams.m_camera_inv_proj, float4(ndc.x, ndc.y, 1.0f, 1.0f)).xyz;
    const half3 direction = mul(u_cbparams.m_camera_inv_view, float4(normalize(lookat), 0)).xyz;

    Payload payload;
    payload.m_miss  = true;
    payload.m_color = half3(0.0h, 0.0h, 0.0h);

    // Setup the ray
    RayDesc ray;
    ray.Origin    = origin;
    ray.Direction = direction;
    ray.TMin      = 0.1f;
    ray.TMax      = 1000.f;

    // Trace the ray
    TraceRay(u_scene_bvh, RAY_FLAG_FORCE_OPAQUE, 0xFF, 0, 0, 0, ray, payload);

    u_output[pixel] = float4(payload.m_color, 1.0f);
}

[shader("closesthit")]
void
ClosestHit(inout Payload payload, const Attributes attrib)
{
    payload.m_miss = false;
    if (u_cbparams.m_mode == RaytraceVisualizeCbParams::ModeInstanceId)
    {
        payload.m_color = color_from_uint(InstanceIndex());
    }
    else if (u_cbparams.m_mode == RaytraceVisualizeCbParams::ModeGeometryId)
    {
        payload.m_color = color_from_uint(GeometryIndex());
    }
    else if (u_cbparams.m_mode == RaytraceVisualizeCbParams::ModeTriangleId)
    {
        payload.m_color = color_from_uint(PrimitiveIndex());
    }
    else if (u_cbparams.m_mode == RaytraceVisualizeCbParams::ModeBaryCentricCoords)
    {
        payload.m_color = half3(attrib.uv.x, attrib.uv.y, 1.0h);
    }
    else if (u_cbparams.m_mode == RaytraceVisualizeCbParams::ModePosition)
    {
        payload.m_color = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
    }
    else if (u_cbparams.m_mode == RaytraceVisualizeCbParams::ModeIndex)
    {
    }
    else
    {
        StandardMaterial mat = u_smc.m_materials[GeometryIndex()];
        payload.m_color = u_textures[mat.m_diffuse_tex_id].SampleLevel(u_sampler, attrib.uv, 0).rgb;
    }
}

[shader("miss")]
void Miss(inout Payload payload) { payload.m_miss = true; }
