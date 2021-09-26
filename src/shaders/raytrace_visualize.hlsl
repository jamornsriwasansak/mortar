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

SamplerState u_sampler : register(s0, space1);
/*
StructuredBuffer<uint16_t>                 u_index_buffer     REGISTER(t1, space1);
StructuredBuffer<float3>                   u_vertex_buffer    REGISTER(t2, space1);
StructuredBuffer<CompactVertex>            u_compactv_buffer  REGISTER(t3, space1);
*/
RaytracingAccelerationStructure u_scene_bvh : register(t0, space1);
StructuredBuffer<BaseInstanceTableEntry> u_base_instance_table : register(t1, space1);
StructuredBuffer<GeometryTableEntry> u_geometry_table : register(t2, space1);
StructuredBuffer<uint16_t> u_indices : register(t3, space1);
StructuredBuffer<float3> u_positions : register(t4, space1);
StructuredBuffer<CompactVertex> u_compact_vertices : register(t5, space1);
StructuredBuffer<StandardMaterial> u_materials : register(t6, space1);
Texture2D<float4> u_textures[100] : register(t7, space1);

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

    [shader("closesthit")] void ClosestHit(inout Payload payload, const Attributes attrib)
{
    const float2 barycentric = attrib.uv;

    payload.m_miss                        = false;
    const uint base_geometry_entry_offset = u_base_instance_table[InstanceID()].m_geometry_entry_offset;
    const uint geometry_offset            = base_geometry_entry_offset + GeometryIndex();
    const GeometryTableEntry geometry_entry = u_geometry_table[geometry_offset];

    const uint index0       = u_indices[PrimitiveIndex() * 3 + geometry_entry.m_index_offset];
    const uint index1       = u_indices[PrimitiveIndex() * 3 + geometry_entry.m_index_offset + 1];
    const uint index2       = u_indices[PrimitiveIndex() * 3 + geometry_entry.m_index_offset + 2];
    const float3 position0  = u_positions[index0 + geometry_entry.m_vertex_offset];
    const float3 position1  = u_positions[index1 + geometry_entry.m_vertex_offset];
    const float3 position2  = u_positions[index2 + geometry_entry.m_vertex_offset];
    const CompactVertex cv0 = u_compact_vertices[index0 + geometry_entry.m_vertex_offset];
    const CompactVertex cv1 = u_compact_vertices[index1 + geometry_entry.m_vertex_offset];
    const CompactVertex cv2 = u_compact_vertices[index2 + geometry_entry.m_vertex_offset];
    const float3 snormal0   = cv0.get_snormal();
    const float3 snormal1   = cv1.get_snormal();
    const float3 snormal2   = cv2.get_snormal();
    const float3 snormal    = normalize(snormal0 * (1.0f - barycentric.x - barycentric.y) +
                                     snormal1 * barycentric.x + snormal2 * barycentric.y);
    const float2 texcoord0  = cv0.m_texcoord;
    const float2 texcoord1  = cv1.m_texcoord;
    const float2 texcoord2  = cv2.m_texcoord;
    const float2 texcoord   = texcoord0 * (1.0f - barycentric.x - barycentric.y) +
                            texcoord1 * barycentric.x + texcoord2 * barycentric.y;
    const StandardMaterial mat = u_materials[geometry_entry.m_material_id];

    const RaytraceVisualizeModeEnum mode = u_cbparams.m_mode;

    if (mode == RaytraceVisualizeModeEnum::ModeInstanceId)
    {
        payload.m_color = color_from_uint(InstanceIndex());
    }
    else if (mode == RaytraceVisualizeModeEnum::ModeBaseInstanceId)
    {
        payload.m_color = color_from_uint(InstanceID());
    }
    else if (mode == RaytraceVisualizeModeEnum::ModeGeometryId)
    {
        payload.m_color = color_from_uint(GeometryIndex());
    }
    else if (mode == RaytraceVisualizeModeEnum::ModeTriangleId)
    {
        payload.m_color = color_from_uint(PrimitiveIndex());
    }
    else if (mode == RaytraceVisualizeModeEnum::ModeBaryCentricCoords)
    {
        payload.m_color = half3(1.0h - attrib.uv.x - attrib.uv.y, attrib.uv.x, attrib.uv.y);
    }
    else if (mode == RaytraceVisualizeModeEnum::ModePosition)
    {
        payload.m_color = half3(WorldRayOrigin() + WorldRayDirection() * RayTCurrent());
    }
    else if (mode == RaytraceVisualizeModeEnum::ModeGeometryNormal)
    {
        payload.m_color = half3(normalize(cross(position1 - position0, position2 - position0)));
    }
    else if (mode == RaytraceVisualizeModeEnum::ModeShadingNormal)
    {
        payload.m_color = half3(snormal);
    }
    else if (mode == RaytraceVisualizeModeEnum::ModeTextureCoords)
    {
        payload.m_color = half3(texcoord, 0.0h);
    }
    else if (mode == RaytraceVisualizeModeEnum::ModeDepth)
    {
        // compute depth
        float depth = length(WorldRayDirection()) * RayTCurrent();
        // tonemap depth and apply contrast
        depth           = depth / (depth + 1.0f);
        depth           = depth * depth * depth;
        payload.m_color = half3(depth, depth, depth);
    }
    else if (mode == RaytraceVisualizeModeEnum::ModeDiffuseReflectance)
    {
        payload.m_color = half3(u_textures[mat.m_diffuse_tex_id].SampleLevel(u_sampler, texcoord, 0).rgb);
    }
    else if (mode == RaytraceVisualizeModeEnum::ModeSpecularReflectance)
    {
        payload.m_color =
            half3(u_textures[mat.m_specular_tex_id].SampleLevel(u_sampler, texcoord, 0).rgb);
    }
    else if (mode == RaytraceVisualizeModeEnum::ModeRoughness)
    {
        payload.m_color =
            half3(u_textures[mat.m_roughness_tex_id].SampleLevel(u_sampler, texcoord, 0).rgb);
    }
}

[shader("miss")] void
Miss(inout Payload payload) { payload.m_miss = true; }
