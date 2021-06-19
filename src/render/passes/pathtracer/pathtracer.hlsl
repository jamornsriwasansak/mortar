#include "../../shared/compactvertex.h"
#include "../../shared/pbrmaterial.h"
#include "mapping.h"
#include "onb.h"

struct PtPayload
{
    float3 m_importance;
    float3 m_radiance;
    float3 m_hit_pos;
    float3 m_inout_dir;
    float2 m_seed2;
    bool m_miss;
};

struct Attributes
{
    float2 uv;
};

cbuffer cb_camera : register(b0)
{
    float4x4 m_inv_view;
    float4x4 m_inv_proj;
};
RWTexture2D<float4> output : register(u0);
SamplerState g_sampler : register(s0);
RaytracingAccelerationStructure SceneBVH : register(t0, space0);
cbuffer materials : register(b1, space0)
{
    PbrMaterial pbr_materials[100];
};
cbuffer material_ids : register(b2, space0)
{
    uint4 ids[25];
};
ByteAddressBuffer indices[100] : register(t3, space0);
StructuredBuffer<CompactVertex> vertices[100] : register(t0, space1);
Texture2D<float4> g_textures[100] : register(t0, space2);

struct VertexAttributes
{
    float3 m_position;
    float3 m_normal;
    float2 m_uv;
};

uint3 get_indices(uint instanceIndex, uint triangleIndex)
{
    const int num_index_per_tri = 3;
    const int num_bytes_per_index = 4;
    int address = triangleIndex * num_index_per_tri * num_bytes_per_index;
    if (address < 0)
        address = 0;
    return indices[instanceIndex].Load3(address);
}

float3 get_interpolated_indices(uint instanceIndex, uint triangleIndex, float3 barycentrics)
{
    uint3 indices = get_indices(instanceIndex, triangleIndex);
    float3 position = float3(0, 0, 0);

    for (uint i = 0; i < 3; i++)
    {
        position += float3(indices[i], indices[i], indices[i]) * barycentrics[i];
    }
    return position;
}

VertexAttributes get_vertex_attributes(uint instanceIndex, uint triangleIndex, float3 barycentrics)
{
    uint3 indices = get_indices(instanceIndex, triangleIndex);
    VertexAttributes v;
    v.m_position = float3(0, 0, 0);
    v.m_normal = float3(0, 0, 0);
    v.m_uv = float2(0, 0);

    for (uint i = 0; i < 3; i++)
    {
        CompactVertex vertex_in = vertices[instanceIndex][indices[i]];
        v.m_position += vertex_in.m_position * barycentrics[i];
        v.m_normal += vertex_in.m_normal * barycentrics[i];
        v.m_uv += float2(vertex_in.m_texcoord_x, vertex_in.m_texcoord_y) * barycentrics[i];
    }

    v.m_normal = normalize(v.m_normal);
    return v;
}

float3 hue(float H)
{
    half R = abs(H * 6 - 3) - 1;
    half G = 2 - abs(H * 6 - 2);
    half B = 2 - abs(H * 6 - 4);
    return saturate(half3(R, G, B));
}

half3 hsv_to_rgb(in half3 hsv)
{
    return half3(((hue(hsv.x) - 1) * hsv.y + 1) * hsv.z);
}

float3 get_color(const int primitive_index)
{
    float h = float(primitive_index % 256) / 256.0f;
    return hsv_to_rgb(half3(h, 1.0f, 1.0f));
}

float rand(const float2 co)
{
    return frac(sin(dot(co, float2(12.9898f, 78.233f))) * 43758.5453f);
}

float2 rand2(const float2 co)
{
    float x = rand(co);
    float y = rand(float2(co.x, x));
    return float2(x, y);
}

[shader("raygeneration")]
void RayGen()
{
    uint2 LaunchIndex = DispatchRaysIndex().xy;
    uint2 LaunchDimensions = DispatchRaysDimensions().xy;

    const float2 uv = (float2(LaunchIndex) + float2(0.5f, 0.5f)) / float2(LaunchDimensions);
    const float2 ndc = uv * 2.0f - 1.0f;

    const float3 origin = mul(m_inv_view, float4(0.0f, 0.0f, 0.0f, 1.0f)).xyz;
    const float3 lookat = mul(m_inv_proj, float4(ndc.x, ndc.y, 1.0f, 1.0f)).xyz;
    const float3 direction = mul(m_inv_view, float4(normalize(lookat), 0)).xyz;

    PtPayload payload;
    payload.m_seed2 = uv;

    float3 result = float3(0.0f, 0.0f, 0.0f);
    for (int i = 0; i < 10; i++)
    {
        payload.m_miss = false;
        payload.m_inout_dir = direction;
        payload.m_importance = float3(1.0f, 1.0f, 1.0f);
        payload.m_radiance = float3(0.0f, 0.0f, 0.0f);
        payload.m_hit_pos = origin;

        for (int i_bounce = 0; i_bounce < 2; i_bounce++)
        {
            // Setup the ray
            RayDesc ray;
            ray.Origin = payload.m_hit_pos;
            ray.Direction = payload.m_inout_dir;
            ray.TMin = 0.1f;
            ray.TMax = 1000.f;

            // Trace the ray
            TraceRay(SceneBVH, RAY_FLAG_FORCE_OPAQUE, 0xFF, 0, 0, 0, ray, payload);
        }

        result += payload.m_radiance;
    }

    output[LaunchIndex.xy] = float4(result / 10, 1.0f);
}

[shader("closesthit")]
void ClosestHit(inout PtPayload payload, const Attributes attrib)
{
    const float3 barycentrics = float3((1.0f - attrib.uv.x - attrib.uv.y), attrib.uv.x, attrib.uv.y);
    const VertexAttributes vattrib = get_vertex_attributes(GeometryIndex(), PrimitiveIndex(), barycentrics);

    // fetch pbr material info
    const uint material_id = ids[GeometryIndex() / 4][GeometryIndex() % 4];
    const PbrMaterial material = pbr_materials[material_id];
    const float3 diffuse_albedo = g_textures[material.m_diffuse_tex_id].SampleLevel(g_sampler, vattrib.m_uv, 0).rgb;

    const Onb onb = Onb_create(vattrib.m_normal);
    const float3 incident_dir = onb.to_local(-payload.m_inout_dir);

    // sample next direction
    const float2 rand = rand2(payload.m_seed2);
    const float3 outgoing_dir = cosine_hemisphere_from_square(rand);

    payload.m_importance *= diffuse_albedo / M_PI;
    payload.m_inout_dir = onb.to_global(outgoing_dir);
    payload.m_seed2 = rand;
    payload.m_hit_pos = vattrib.m_position;
}

[shader("miss")]
void Miss(inout PtPayload payload)
{
    payload.m_radiance += payload.m_importance * float3(1.0f, 1.0f, 1.0f) * 10.0f;
    payload.m_miss = true;
}
