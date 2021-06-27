#include "../../common/compact_vertex.h"
#include "../../common/mapping.h"
#include "../../common/onb.h"
#include "../../common/shared.h"
#include "../../common/standard_emissive_ref.h"
#include "../../common/standard_material.h"
#include "../../common/standard_material_ref.h"
#include "../../rng/pcg.h"
#include "bindless_object_table.h"
#include "direct_light_params.h"
#include "reservior.h"

struct PtPayload
{
    float3 m_importance;
    float3 m_radiance;
    float3 m_hit_pos;
    float3 m_inout_dir;
    PcgRng m_rng;
    bool   m_miss;
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

struct StandardMaterialContainer
{
    StandardMaterial materials[100];
};

struct StandardEmissiveContainer
{
    StandardEmissive emissives[100];
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
ConstantBuffer<CameraInfo> u_camera                REGISTER(b0, space0);
ConstantBuffer<DirectLightParams> u_params         REGISTER(b1, space0);
StructuredBuffer<Reservior> u_prev_frame_reservior REGISTER(t0, space0);
RWTexture2D<float4> u_output                       REGISTER(u0, space0);
RWStructuredBuffer<Reservior> u_frame_reservior    REGISTER(u1, space0);

// space 1
// bindless material
SamplerState u_sampler                                    REGISTER(s0, space1);
ConstantBuffer<StandardMaterialContainer> u_pbr_materials REGISTER(b0, space1);
ConstantBuffer<MaterialIdContainer> u_material_ids        REGISTER(b1, space1);
StructuredBuffer<StandardEmissive> u_emissives            REGISTER(t0, space1);
Texture2D<float4>                                         u_textures[100] REGISTER(t1, space1);

// space 2 & 3
// bindless mesh info
RaytracingAccelerationStructure u_scene_bvh               REGISTER(t0, space2);
ConstantBuffer<BindlessObjectTable> u_bindless_mesh_table REGISTER(b0, space2);
ConstantBuffer<NumTrianglesContainer> u_num_triangles     REGISTER(b1, space2);
ByteAddressBuffer                                         u_index_buffers[100] REGISTER(t1, space2);
StructuredBuffer<CompactVertex> u_vertex_buffers[100] REGISTER(t0, space3);

struct VertexAttributes
{
    float3 m_position;
    float3 m_snormal;
    float3 m_gnormal;
    float2 m_uv;
};

uint3
get_indices(uint instanceIndex, uint triangleIndex)
{
    const int num_index_per_tri   = 3;
    const int num_bytes_per_index = 4;
    int       address             = triangleIndex * num_index_per_tri * num_bytes_per_index;
    if (address < 0) address = 0;
    return u_index_buffers[instanceIndex].Load3(address);
}

float3
get_interpolated_indices(uint instanceIndex, uint triangleIndex, float3 barycentrics)
{
    uint3  u_index_buffers = get_indices(instanceIndex, triangleIndex);
    float3 position        = float3(0, 0, 0);

    for (uint i = 0; i < 3; i++)
    {
        position += float3(u_index_buffers[i], u_index_buffers[i], u_index_buffers[i]) * barycentrics[i];
    }
    return position;
}

VertexAttributes
get_vertex_attributes(uint instanceIndex, uint triangleIndex, float3 barycentrics)
{
    uint3            u_index_buffers = get_indices(instanceIndex, triangleIndex);
    VertexAttributes v;
    v.m_position = float3(0, 0, 0);
    v.m_snormal  = float3(0, 0, 0);
    v.m_uv       = float2(0, 0);

    float3 e0;
    float3 e1;
    [[unroll]]

    for (uint i = 0; i < 3; i++)
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

float3
hue(float H)
{
    half R = abs(H * 6 - 3) - 1;
    half G = 2 - abs(H * 6 - 2);
    half B = 2 - abs(H * 6 - 4);
    return saturate(half3(R, G, B));
}

half3
hsv_to_rgb(const half3 hsv)
{
    return half3(((hue(hsv.x) - 1) * hsv.y + 1) * hsv.z);
}

float3
get_color(const int primitive_index)
{
    float h = float(primitive_index % 256) / 256.0f;
    return hsv_to_rgb(half3(h, 1.0f, 1.0f));
}

float
rand(const float2 co)
{
    return min(frac(sin(dot(co, float2(12.9898f, 78.233f))) * 43758.5453f) + 0.0000000001f, 1.0f);
}

float2
rand2(const float2 co)
{
    float x = rand(co);
    float y = rand(float2(co.x, x));
    return float2(x, y);
}

float
sqr(float x)
{
    return x * x;
}

float
length2(const float3 x)
{
    return dot(x, x);
}

float
connect(const float3 diff, const float3 normal1, const float3 normal2)
{
    return max(dot(diff, normal1), 0.0f) * max(-dot(diff, normal2), 0.0f) / sqr(length2(diff));
}

struct LightSample
{
    float3 m_emission;
    float3 m_position;
    float3 m_snormal;
    float3 m_gnormal;
};

void
sample_emissive_geometry(INOUT(uint) geometry_id, INOUT(uint) primitive_id, INOUT(half2) uv, const float2 random0, const float2 random1)
{
    const uint emittor_id    = random0.x * u_bindless_mesh_table.m_num_emissive_objects;
    geometry_id              = emittor_id + u_bindless_mesh_table.m_emissive_object_start_index;
    const uint num_triangles = u_num_triangles.m_num_triangles[emittor_id / 4][emittor_id % 4];
    primitive_id             = int(random0.y * num_triangles);
    uv                       = triangle_from_square(random1);
}

LightSample
sample_light(const uint geometry_id, const uint primitive_id, const half2 uv)
{
    const float3           uvw    = float3(uv.x, uv.y, 1.0f - uv.x - uv.y);
    const VertexAttributes attrib = get_vertex_attributes(geometry_id, primitive_id, uvw);

    LightSample light_sample;
    light_sample.m_position = attrib.m_position;
    light_sample.m_snormal  = attrib.m_snormal;
    light_sample.m_gnormal  = attrib.m_gnormal;

    // fetch texture value from texel
    StandardEmissive emissive = u_emissives[0];
    light_sample.m_emission =
        u_textures[emissive.m_emissive_tex_id].SampleLevel(u_sampler, attrib.m_uv, 0).rgb * emissive.m_emissive_scale;

    return light_sample;
}

float3
eval_connection(const VertexAttributes hit, const LightSample light_sample, const float3 incident, const StandardMaterialInfo material)
{
    const float3 diff          = light_sample.m_position - hit.m_position;
    const float  geometry_term = max(dot(diff, hit.m_snormal), 0.0f) *
                                max(-dot(diff, light_sample.m_snormal), 0.0f) / sqr(length2(diff));
    return material.eval(normalize(diff), incident) * geometry_term * light_sample.m_emission;
}

SHADER_TYPE("raygeneration")
void
RayGen()
{
    uint2 pixel       = DispatchRaysIndex().xy;
    uint2 resolution  = DispatchRaysDimensions().xy;
    uint  pixel_index = pixel.y * resolution.x + pixel.x;

    const float2 uv  = (float2(pixel) + float2(0.5f, 0.5f)) / float2(resolution);
    const float2 ndc = uv * 2.0f - 1.0f;

    const float3 origin    = mul(u_camera.m_inv_view, float4(0.0f, 0.0f, 0.0f, 1.0f)).xyz;
    const float3 lookat    = mul(u_camera.m_inv_proj, float4(ndc.x, ndc.y, 1.0f, 1.0f)).xyz;
    const float3 direction = mul(u_camera.m_inv_view, float4(normalize(lookat), 0)).xyz;

    PtPayload payload;
    payload.m_rng.init(pixel_index, u_params.m_rng_stream_id);

    const int num_samples = 1;
    float3    result      = float3(0.0f, 0.0f, 0.0f);
    for (int i = 0; i < num_samples; i++)
    {
        payload.m_inout_dir  = direction;
        payload.m_miss       = false;
        payload.m_importance = float3(1.0f, 1.0f, 1.0f);
        payload.m_radiance   = float3(0.0f, 0.0f, 0.0f);
        payload.m_hit_pos    = origin;

        for (int i_bounce = 0; i_bounce < 1; i_bounce++)
        {
            // Setup the ray
            RayDesc ray;
            ray.Origin    = payload.m_hit_pos;
            ray.Direction = payload.m_inout_dir;
            ray.TMin      = 0.1f;
            ray.TMax      = 1000.f;

            // Trace the ray
            if (!payload.m_miss)
            {
                TraceRay(u_scene_bvh, RAY_FLAG_FORCE_OPAQUE, 0xFF, 0, 0, 0, ray, payload);
            }
        }

        result += payload.m_radiance;
    }

    if (false)
    {
        float4 input                = u_output[pixel];
        float3 combined_contrib     = input.xyz * input.w + result;
        float  combined_num_samples = input.w + num_samples;
        u_output[pixel] = float4(combined_contrib / float(combined_num_samples), combined_num_samples);
    }
    else
    {
        u_output[pixel] = float4(result, 1.0f);
    }
}

SHADER_TYPE("closesthit") void ClosestHit(INOUT(PtPayload) payload, const Attributes attrib)
{
    uint2 pixel       = DispatchRaysIndex().xy;
    uint2 resolution  = DispatchRaysDimensions().xy;
    uint  pixel_index = pixel.y * resolution.x + pixel.x;

    uint64_t rng_inc = payload.m_rng.get_inc(u_params.m_rng_stream_id);

    const float3 barycentrics = float3((1.0f - attrib.uv.x - attrib.uv.y), attrib.uv.x, attrib.uv.y);
    const VertexAttributes vattrib = get_vertex_attributes(GeometryIndex(), PrimitiveIndex(), barycentrics);

    // fetch pbr material info
    const uint material_id = u_material_ids.m_ids[GeometryIndex() / 4][GeometryIndex() % 4];
    const StandardMaterial material = u_pbr_materials.materials[material_id];
    const float3           diff_refl =
        u_textures[material.m_diffuse_tex_id].SampleLevel(u_sampler, vattrib.m_uv, 0).rgb;
    const float3 spec_refl =
        u_textures[material.m_specular_tex_id].SampleLevel(u_sampler, vattrib.m_uv, 0).rgb;
    const float roughness =
        u_textures[material.m_roughness_tex_id].SampleLevel(u_sampler, vattrib.m_uv, 0).r;

    StandardMaterialInfo material_info;
    material_info.init(float3(1.0f, 1.0f, 1.0f), spec_refl, roughness);

    const Onb    onb                = Onb_create(vattrib.m_gnormal);
    const float3 local_incident_dir = onb.to_local(-payload.m_inout_dir);

    // sample next direction
    const float3 local_outgoing_dir = cosine_hemisphere_from_square(payload.m_rng.next_float2(rng_inc));

    int       num_samples       = 8;
    int       total_num_samples = 0;
    Reservior reservior;
    reservior.init();

    {
        float3 selected_diff;
        float3 selected_nee;
        float  selected_weight;
        for (int i = 0; i < num_samples; i++)
        {
            const float2 random0 = payload.m_rng.next_float2(rng_inc);
            const float2 random1 = payload.m_rng.next_float2(rng_inc);

            // sample geometry
            uint  geometry_id;
            uint  primitive_id;
            half2 uv;
            sample_emissive_geometry(geometry_id, primitive_id, uv, random0, random1);

            const LightSample light_sample = sample_light(geometry_id, primitive_id, uv);
            const float3      diff         = light_sample.m_position - vattrib.m_position;
            const float3 nee = eval_connection(vattrib, light_sample, local_incident_dir, material_info);
            const float weight = length(nee);

            float random = payload.m_rng.next_float(rng_inc);
            if (reservior.update(geometry_id, primitive_id, uv, weight, random))
            {
                selected_nee    = nee;
                selected_weight = weight;
                selected_diff   = diff;
            }
        }

        LightSample light_sample =
            sample_light(reservior.m_geometry_id, reservior.m_primitive_id, reservior.m_uv);
        const float3 nee = eval_connection(vattrib, light_sample, local_incident_dir, material_info);
        const float  weight = length(nee);
        total_num_samples += num_samples;

        // evaluate visibility
        RayDesc ray;
        ray.Origin    = vattrib.m_position;
        ray.Direction = selected_diff;
        ray.TMin      = 0.001f;
        ray.TMax      = 0.999f;
        PtPayload shadow_payload;
        shadow_payload.m_miss = false;
        TraceRay(u_scene_bvh,
                 RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER,
                 0xFF,
                 0,
                 0,
                 0,
                 ray,
                 shadow_payload);
        reservior.m_w_sum *= shadow_payload.m_miss;
    }

    u_frame_reservior[pixel_index] = reservior;

    int radius = 1;
    for (int i = -radius; i <= radius; i++)
        for (int j = -radius; j <= radius; j++)
        {
            int2      offset_pixel       = clamp(pixel + int2(i, j), int2(0, 0), resolution);
            int       offset_pixel_index = offset_pixel.y * resolution.x + offset_pixel.x;
            Reservior prev_reservior     = u_prev_frame_reservior[offset_pixel_index];
            total_num_samples += num_samples;

            reservior.update(prev_reservior.m_geometry_id,
                             prev_reservior.m_primitive_id,
                             prev_reservior.m_uv,
                             prev_reservior.m_w_sum,
                             payload.m_rng.next_float(rng_inc));
        }

    LightSample light_sample =
        sample_light(reservior.m_geometry_id, reservior.m_primitive_id, reservior.m_uv);
    const float3 nee    = eval_connection(vattrib, light_sample, local_incident_dir, material_info);
    const float  weight = length(nee);
    const float  inv_pdf = weight > 0.0f ? reservior.m_w_sum / (weight * total_num_samples) : 0.0f;

    const float3 nee_contrib = diff_refl * payload.m_importance * nee * inv_pdf;
    payload.m_importance *= diff_refl / M_PI;
    payload.m_inout_dir = onb.to_global(local_outgoing_dir);
    payload.m_hit_pos   = vattrib.m_position;
    payload.m_radiance += nee_contrib + diff_refl * 0.001f;
}

SHADER_TYPE("closesthit")
void
EmissiveClosestHit(INOUT(PtPayload) payload, const Attributes attrib)
{
    payload.m_radiance += 500.0f;
}

SHADER_TYPE("miss")
void Miss(INOUT(PtPayload) payload)
{
    // payload.m_radiance += payload.m_importance * float3(1.0f, 1.0f, 1.0f) * 1.0f;
    payload.m_miss = true;
}

SHADER_TYPE("miss")
void ShadowMiss(INOUT(PtPayload) payload) { payload.m_miss = true; }
