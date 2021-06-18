#include "../../shared/compactvertex.h"
#include "../../shared/pbrmaterial.h"

struct HitInfo
{
    float4 ShadedColorAndHitT;
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
RWTexture2D<float4> RTOutput : register(u0);
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
    float3 position;
    float3 normal;
    float2 uv;
};

uint3 GetIndices(uint instanceIndex, uint triangleIndex)
{
    int address = ((triangleIndex * 3)) * 4;
    if (address < 0)
        address = 0;
    return indices[instanceIndex].Load3(address);
    /*
    int address = (triangleIndex) * 3 * 4;
    return indices[0].Load3(address);
    */
//    return indices.Load3(address);
}

float3 GetInterpolatedIndices(uint instanceIndex, uint triangleIndex, float3 barycentrics)
{
    uint3 indices = GetIndices(instanceIndex, triangleIndex);
    float3 position = float3(0, 0, 0);

    for (uint i = 0; i < 3; i++)
    {
        position += float3(indices[i], indices[i], indices[i]) * barycentrics[i];
    }
    return position;
}

VertexAttributes GetVertexAttributes(uint instanceIndex, uint triangleIndex, float3 barycentrics)
{
    uint3 indices = GetIndices(instanceIndex, triangleIndex);
    VertexAttributes v;
    v.position = float3(0, 0, 0);
    v.normal = float3(0, 0, 0);
    v.uv = float2(0, 0);

    for (uint i = 0; i < 3; i++)
    {
        CompactVertex vertex_in = vertices[instanceIndex][indices[i]];
        v.position += vertex_in.m_position * barycentrics[i];
        v.normal += vertex_in.m_normal * barycentrics[i];
        v.uv += float2(vertex_in.m_texcoord_x, vertex_in.m_texcoord_y) * barycentrics[i];
    }

    v.normal = normalize(v.normal);
    return v;
}

float3 Hue(float H)
{
    half R = abs(H * 6 - 3) - 1;
    half G = 2 - abs(H * 6 - 2);
    half B = 2 - abs(H * 6 - 4);
    return saturate(half3(R, G, B));
}

half3 hsv_to_rgb(in half3 hsv)
{
    return half3(((Hue(hsv.x) - 1) * hsv.y + 1) * hsv.z);
}

float3 get_color(const int primitive_index)
{
    float h = float(primitive_index % 256) / 256.0f;
    return hsv_to_rgb(half3(h, 1.0f, 1.0f));
}