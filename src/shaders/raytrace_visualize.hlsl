#include "common/color_conversion.h"
#include "raytrace_visualize_params.h"
#include "shared/bindless_table.h"
#include "shared/camera_params.h"
#include "shared/compact_vertex.h"
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

ConstantBuffer<RaytraceVisualizeCbParams> u_cbparams : register(b0, space0);
RWTexture2D<float4> u_output : register(u0, space0);

struct StandardMaterialsContainer
{
    StandardMaterial m_materials[100];
};

SamplerState u_sampler : register(s0, space1);
ConstantBuffer<StandardMaterialsContainer> u_smc : register(b0, space1);
/*
StructuredBuffer<uint16_t>                 u_index_buffer     REGISTER(t1, space1);
StructuredBuffer<float3>                   u_vertex_buffer    REGISTER(t2, space1);
StructuredBuffer<CompactVertex>            u_compactv_buffer  REGISTER(t3, space1);
*/
RaytracingAccelerationStructure u_scene_bvh : register(t0, space1);
StructuredBuffer<BaseInstanceTableEntry> u_base_instance_table : register(t1, space1);
StructuredBuffer<GeometryTableEntry> u_geometry_table : register(t2, space1);
StructuredBuffer<uint16_t> u_indices : register(t3, space1);
StructuredBuffer<CompactVertex> u_compact_vertices : register(t4, space1);
Texture2D<float4> u_textures[100] : register(t5, space1);

[shader("raygeneration")] void
RayGen()
{
    uint2 pixel      = DispatchRaysIndex().xy;
    uint2 resolution = DispatchRaysDimensions().xy;
    uint pixel_index = pixel.y * resolution.x + pixel.x;

    const float2 uv  = (float2(pixel) + float2(0.5f, 0.5f)) / float2(resolution);
    const float2 ndc = uv * 2.0f - 1.0f;

    const float3 origin = mul(u_cbparams.m_camera_inv_view, float4(0.0f, 0.0f, 0.0f, 1.0f)).xyz;
    const float3 lookat = mul(u_cbparams.m_camera_inv_proj, float4(ndc.x, ndc.y, 1.0f, 1.0f)).xyz;
    const float3 direction = mul(u_cbparams.m_camera_inv_view, float4(normalize(lookat), 0.0f)).xyz;

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
void ClosestHit(inout Payload payload, const Attributes attrib)
{
    payload.m_miss                        = false;
    const uint base_geometry_entry_offset = u_base_instance_table[InstanceID()].m_geometry_entry_offset;
    const uint geometry_offset            = base_geometry_entry_offset + GeometryIndex();
    const GeometryTableEntry geometry_entry = u_geometry_table[geometry_offset];

    const uint index0       = u_indices[PrimitiveIndex() * 3 + geometry_entry.m_index_offset];
    const uint index1       = u_indices[PrimitiveIndex() * 3 + 1 + geometry_entry.m_index_offset];
    const uint index2       = u_indices[PrimitiveIndex() * 3 + 2 + geometry_entry.m_index_offset];
    const CompactVertex cv0 = u_compact_vertices[index0 + geometry_entry.m_vertex_offset];
    const CompactVertex cv1 = u_compact_vertices[index1 + geometry_entry.m_vertex_offset];
    const CompactVertex cv2 = u_compact_vertices[index2 + geometry_entry.m_vertex_offset];

    if (u_cbparams.m_mode == RaytraceVisualizeCbParams::ModeInstanceId)
    {
        payload.m_color = color_from_uint(InstanceIndex());
    }
    else if (u_cbparams.m_mode == RaytraceVisualizeCbParams::ModeInstanceId)
    {
        payload.m_color = color_from_uint(InstanceID());
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
        payload.m_color = half3(1.0h - attrib.uv.x - attrib.uv.y, attrib.uv.x, attrib.uv.y);
    }
    else if (u_cbparams.m_mode == RaytraceVisualizeCbParams::ModePosition)
    {
        payload.m_color = half3(WorldRayOrigin() + WorldRayDirection() * RayTCurrent());
    }
    else if (u_cbparams.m_mode == RaytraceVisualizeCbParams::ModeTextureCoords)
    {
        const half2 texcoord0 = half2(cv0.m_texcoord_x, cv0.m_texcoord_y);
        const half2 texcoord1 = half2(cv1.m_texcoord_x, cv1.m_texcoord_y);
        const half2 texcoord2 = half2(cv2.m_texcoord_x, cv2.m_texcoord_y);
        const half2 barycentric = half2(attrib.uv);
        const half2 texcoord  =
            texcoord0 * (1.0h - barycentric.x - barycentric.y)
            + texcoord1 * barycentric.x
            + texcoord2 * barycentric.y;
        payload.m_color = color_from_uint(geometry_entry.m_material_id) * 0.0001h +
                          half3(texcoord, 0.0h);
    }
    else
    {
        StandardMaterial mat = u_smc.m_materials[GeometryIndex()];
        payload.m_color =
            half3(u_textures[mat.m_diffuse_tex_id].SampleLevel(u_sampler, attrib.uv, 0).rgb);
    }
}

[shader("miss")] void
Miss(inout Payload payload) { payload.m_miss = true; }
