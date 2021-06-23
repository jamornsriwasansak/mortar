#include "../../shared/compactvertex.h"
#include "../../shared/pbrmaterial.h"
#include "reservior.h"
#include "bindlessobjectable.h"
#include "mapping.h"
#include "onb.h"
#include "directlight_params.h"

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

struct CameraInfo
{
    float4x4 m_inv_view;
    float4x4 m_inv_proj;
};

struct PbrMaterialContainer
{
    PbrMaterial materials[100];
};

struct MaterialIdContainer
{
    uint4 m_ids[25];
};

struct NumTrianglesContainer
{
    uint4 m_num_triangles[25];
};

// space 0
ConstantBuffer<CameraInfo>            u_camera : register(b0, space0);
ConstantBuffer<DirectLightParams>     u_params : register(b1, space0);
StructuredBuffer<Reservior>           u_prev_frame_reservior : register(t0, space0);
RWTexture2D<float4>                   u_output : register(u0, space0);
RWStructuredBuffer<Reservior>         u_frame_reservior : register(u1, space0);

// space 1
// bindless material
SamplerState                          u_sampler : register(s0, space1);
ConstantBuffer<PbrMaterialContainer>  u_pbr_materials : register(b0, space1);
ConstantBuffer<MaterialIdContainer>   u_material_ids : register(b1, space1);
Texture2D<float4>                     u_textures[100] : register(t0, space1);

// space 2 & 3
// bindless mesh info
RaytracingAccelerationStructure       u_scene_bvh : register(t0, space2);
ConstantBuffer<BindlessObjectTable>   u_bindless_mesh_table : register(b0, space2);
ConstantBuffer<NumTrianglesContainer> u_num_triangles : register(b1, space2);
ByteAddressBuffer                     u_index_buffers[100] : register(t1, space2);
StructuredBuffer<CompactVertex>       u_vertex_buffers[100] : register(t0, space3);

struct VertexAttributes
{
    float3 m_position;
    float3 m_snormal;
    float3 m_gnormal;
    float2 m_uv;
};

uint3 get_indices(uint instanceIndex, uint triangleIndex)
{
    const int num_index_per_tri = 3;
    const int num_bytes_per_index = 4;
    int address = triangleIndex * num_index_per_tri * num_bytes_per_index;
    if (address < 0)
        address = 0;
    return u_index_buffers[instanceIndex].Load3(address);
}

float3 get_interpolated_indices(uint instanceIndex, uint triangleIndex, float3 barycentrics)
{
    uint3 u_index_buffers = get_indices(instanceIndex, triangleIndex);
    float3 position = float3(0, 0, 0);

    for (uint i = 0; i < 3; i++)
    {
        position += float3(u_index_buffers[i], u_index_buffers[i], u_index_buffers[i]) * barycentrics[i];
    }
    return position;
}

VertexAttributes get_vertex_attributes(uint instanceIndex, uint triangleIndex, float3 barycentrics)
{
    uint3 u_index_buffers = get_indices(instanceIndex, triangleIndex);
    VertexAttributes v;
    v.m_position = float3(0, 0, 0);
    v.m_snormal = float3(0, 0, 0);
    v.m_uv = float2(0, 0);

    float3 e0;
    float3 e1;
    [[unroll]]for (uint i = 0; i < 3; i++)
    {
        CompactVertex vertex_in = u_vertex_buffers[instanceIndex][u_index_buffers[i]];
        v.m_position += vertex_in.m_position * barycentrics[i];
        v.m_snormal += vertex_in.m_snormal * barycentrics[i];
        v.m_uv += float2(vertex_in.m_texcoord_x, vertex_in.m_texcoord_y) * barycentrics[i];

        if (i == 0)
        {
            e0 = vertex_in.m_position;
            e1 = vertex_in.m_position;
        }
        else if (i == 1)
        {
            e0 -= vertex_in.m_position;
        }
        else if (i == 2)
        {
            e1 -= vertex_in.m_position;
        }
    }

    v.m_snormal = normalize(v.m_snormal);
    v.m_gnormal = normalize(cross(e0, e1));
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
    return min(frac(sin(dot(co, float2(12.9898f, 78.233f))) * 43758.5453f) + 0.0000000001f, 1.0f);
}

float2 rand2(const float2 co)
{
    float x = rand(co);
    float y = rand(float2(co.x, x));
    return float2(x, y);
}

float sqr(float x)
{
    return x * x;
}

float length2(const float3 x)
{
    return dot(x, x);
}

float connect(const float3 diff, const float3 normal1, const float3 normal2)
{
    return max(dot(diff, normal1), 0.0f) * max(-dot(diff, normal2), 0.0f) / length2(diff);
}

[shader("raygeneration")]
void RayGen()
{
    uint2 LaunchIndex = DispatchRaysIndex().xy;
    uint2 LaunchDimensions = DispatchRaysDimensions().xy;

    const float2 uv = (float2(LaunchIndex) + float2(0.5f, 0.5f)) / float2(LaunchDimensions);
    const float2 ndc = uv * 2.0f - 1.0f;

    const float3 origin = mul(u_camera.m_inv_view, float4(0.0f, 0.0f, 0.0f, 1.0f)).xyz;
    const float3 lookat = mul(u_camera.m_inv_proj, float4(ndc.x, ndc.y, 1.0f, 1.0f)).xyz;
    const float3 direction = mul(u_camera.m_inv_view, float4(normalize(lookat), 0)).xyz;

    PtPayload payload;
    payload.m_seed2 = uv + u_params.m_rng_offset / 1024.0f;

    float3 result = float3(0.0f, 0.0f, 0.0f);
    for (int i = 0; i < 1; i++)
    {
        payload.m_inout_dir = direction;
        payload.m_miss = false;
        payload.m_importance = float3(1.0f, 1.0f, 1.0f);
        payload.m_radiance = float3(0.0f, 0.0f, 0.0f);
        payload.m_hit_pos = origin;

        for (int i_bounce = 0; i_bounce < 1; i_bounce++)
        {
            // Setup the ray
            RayDesc ray;
            ray.Origin = payload.m_hit_pos;
            ray.Direction = payload.m_inout_dir;
            ray.TMin = 0.1f;
            ray.TMax = 1000.f;

            // Trace the ray
            if (!payload.m_miss)
            {
                TraceRay(u_scene_bvh, RAY_FLAG_FORCE_OPAQUE, 0xFF, 0, 0, 0, ray, payload);
            }
        }

        result += payload.m_radiance;
    }

    u_output[LaunchIndex.xy] = float4(result / 1.0f, 1.0f);
}

[shader("closesthit")]
void ClosestHit(inout PtPayload payload, const Attributes attrib)
{
    const float3 barycentrics = float3((1.0f - attrib.uv.x - attrib.uv.y), attrib.uv.x, attrib.uv.y);
    const VertexAttributes vattrib = get_vertex_attributes(GeometryIndex(), PrimitiveIndex(), barycentrics);

    // fetch pbr material info
    const uint material_id = u_material_ids.m_ids[GeometryIndex() / 4][GeometryIndex() % 4];
    const PbrMaterial material = u_pbr_materials.materials[material_id];
    const float3 diffuse_reflectance = u_textures[material.m_diffuse_tex_id].SampleLevel(u_sampler, vattrib.m_uv, 0).rgb;
    const float3 specular_reflectance = u_textures[material.m_specular_tex_id].SampleLevel(u_sampler, vattrib.m_uv, 0).rgb;
    const float specular_roughness = u_textures[material.m_roughness_tex_id].SampleLevel(u_sampler, vattrib.m_uv, 0).r;

    const Onb onb = Onb_create(vattrib.m_gnormal);
    const float3 local_incident_dir = onb.to_local(-payload.m_inout_dir);

    // sample next direction
    float2 samples = rand2(payload.m_seed2);
    const float3 local_outgoing_dir = cosine_hemisphere_from_square(samples);

    // fetch prev frame reservior
    uint2 pixel = DispatchRaysIndex().xy;
    uint2 resolution = DispatchRaysDimensions().xy;
    uint pixel_index = pixel.y * resolution.x + pixel.x;
    Reservior prev_frame_reservior = u_prev_frame_reservior[pixel_index];

    Reservior reservior = Reservior_create();
    float3 nee;
    {
        VertexAttributes vattrib2;
        for (int i = 0; i < 16; i++)
        {
            // sample triangle index
            samples = rand2(samples);
            float2 samples0 = samples;
            samples = rand2(samples);
            float2 samples1 = samples;
            samples = rand2(samples);
            float2 samples2 = samples;
            const uint emissive_id = samples0.x * u_bindless_mesh_table.m_num_emissive_objects;
            const uint geometry_id = emissive_id + u_bindless_mesh_table.m_emissive_object_start_index;
            const uint num_triangles = u_num_triangles.m_num_triangles[emissive_id / 4][emissive_id % 4];
            const uint primitive_id = int(samples0.y * num_triangles);
            const float2 uv = triangle_from_square(samples1);
            const float3 uvw = float3(uv.x, uv.y, 1.0f - uv.x - uv.y);

            const VertexAttributes candidate_attrib = get_vertex_attributes(geometry_id, primitive_id, uvw);
            const float3 diff = candidate_attrib.m_position - vattrib.m_position;
            const float candidate_connection_contrib = connect(diff, vattrib.m_snormal, candidate_attrib.m_snormal);
            const float weight = candidate_connection_contrib;

            if (reservior.update(float3(1.0f, 1.0f, 1.0f) * candidate_connection_contrib, candidate_connection_contrib, samples2.x))
            {
                vattrib2 = candidate_attrib;
            }
        }

        reservior.m_w = reservior.m_w_sum > 0.0f ? reservior.m_w_sum / (reservior.m_p_y * reservior.m_m) : 1.0f;
        const float3 diff = vattrib2.m_position - vattrib.m_position;

        // Setup the ray
        RayDesc ray;
        ray.Origin = vattrib.m_position;
        ray.Direction = diff;
        ray.TMin = 0.001f;
        ray.TMax = 0.999f;
        PtPayload shadow_payload;
        shadow_payload.m_miss = false;
        TraceRay(u_scene_bvh, RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER, 0xFF, 0, 0, 0, ray, shadow_payload);

        // update reservior
        if (!shadow_payload.m_miss)
        {
            reservior.m_y = float3(0.0f, 0.0f, 0.0f);
        }

    }

    samples = rand2(samples);

    /*
    Reservior combined_reservior = Reservior_create();
    combined_reservior.update(reservior.m_y, reservior.m_p_y, reservior.m_y * reservior.m_w, samples.x);
    combined_reservior.update(prev_reservior.m_y, prev_reservior.m_p_y, prev_reservior.m_y * prev_reservior.m_w, samples.y);

    combined_reservior.m_m = reservior.m_m + prev_reservior.m_m;
    combined_reservior.m_p_y = reservior.m_p_y + prev_reservior.m_p_y;
*/

    // nee
    nee = payload.m_importance * reservior.m_y * reservior.m_w;

    payload.m_importance *= diffuse_reflectance / M_PI;
    payload.m_inout_dir = onb.to_global(local_outgoing_dir);
    payload.m_seed2 = samples;
    payload.m_hit_pos = vattrib.m_position;
    payload.m_radiance += nee * diffuse_reflectance * 10.0f + diffuse_reflectance * 0.1f;

    u_frame_reservior[pixel_index] = prev_frame_reservior;
}

[shader("closesthit")]
void EmissiveClosestHit(inout PtPayload payload, const Attributes attrib)
{
    payload.m_radiance += 10.0f;
}

[shader("miss")]
void Miss(inout PtPayload payload)
{
    //payload.m_radiance += payload.m_importance * float3(1.0f, 1.0f, 1.0f) * 1.0f;
    payload.m_miss = true;
}

[shader("miss")]
void ShadowMiss(inout PtPayload payload)
{
    payload.m_miss = true;
}
