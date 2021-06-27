#include "../../common/compact_vertex.h"
#include "../../common/mapping.h"
#include "../../common/onb.h"
#include "../../common/shared.h"
#include "../../common/standard_emissive_ref.h"
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

LightSample
sample_light(const float2 random0, const float2 random1)
{
    const uint   emittor_id    = random0.x * u_bindless_mesh_table.m_num_emissive_objects;
    const uint   geometry_id   = emittor_id + u_bindless_mesh_table.m_emissive_object_start_index;
    const uint   num_triangles = u_num_triangles.m_num_triangles[emittor_id / 4][emittor_id % 4];
    const uint   primitive_id  = int(random0.y * num_triangles);
    const float2 uv            = triangle_from_square(random1);
    const float3 uvw           = float3(uv.x, uv.y, 1.0f - uv.x - uv.y);

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
    const float3           diffuse_reflectance =
        u_textures[material.m_diffuse_tex_id].SampleLevel(u_sampler, vattrib.m_uv, 0).rgb;
    const float3 specular_reflectance =
        u_textures[material.m_specular_tex_id].SampleLevel(u_sampler, vattrib.m_uv, 0).rgb;
    const float specular_roughness =
        u_textures[material.m_roughness_tex_id].SampleLevel(u_sampler, vattrib.m_uv, 0).r;

    const Onb    onb                = Onb_create(vattrib.m_gnormal);
    const float3 local_incident_dir = onb.to_local(-payload.m_inout_dir);

    // sample next direction
    const float3 local_outgoing_dir = cosine_hemisphere_from_square(payload.m_rng.next_float2(rng_inc));

    int       num_samples = 1;
    Reservior reservior;
    reservior.init();

    float3 nee;
    {
        LightSample selected_light_sample;
        for (int i = 0; i < num_samples; i++)
        {
            float2      random0      = payload.m_rng.next_float2(rng_inc);
            float2      random1      = payload.m_rng.next_float2(rng_inc);
            LightSample light_sample = sample_light(random0, random1);

            const float3 diff                        = light_sample.m_position - vattrib.m_position;
            const float candidate_connection_contrib = connect(diff, vattrib.m_snormal, light_sample.m_snormal);
            const float weight                       = candidate_connection_contrib;

            float random = payload.m_rng.next_float(rng_inc);
            if (reservior.update(candidate_connection_contrib * light_sample.m_emission,
                                 candidate_connection_contrib,
                                 candidate_connection_contrib,
                                 1.0f,
                                 random))
            {
                selected_light_sample = light_sample;
            }
        }

        reservior.m_inv_pdf = reservior.m_w_sum > 0.0f
                                  ? reservior.m_w_sum / (reservior.m_p_y * reservior.m_num_samples)
                                  : 1.0f;
        const float3 diff   = selected_light_sample.m_position - vattrib.m_position;

        // Setup the ray
        RayDesc ray;
        ray.Origin    = vattrib.m_position;
        ray.Direction = diff;
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

        // update reservior
        if (!shadow_payload.m_miss)
        {
            reservior.m_y     = float3(0.0f, 0.0f, 0.0f);
            reservior.m_w_sum = 0.0f;
        }
    }

    // fetch prev frame reservior
    Reservior combined_reservior;
    combined_reservior.init();

    combined_reservior.update(reservior.m_y,
                              reservior.m_p_y,
                              reservior.m_w_sum,
                              reservior.m_num_samples,
                              payload.m_rng.next_float(rng_inc));
    int radius = 0;
    for (int i = -radius; i <= radius; i++)
        for (int j = -radius; j <= radius; j++)
        {
            int2      offset_pixel       = clamp(pixel + int2(i, j), int2(0, 0), resolution);
            int       offset_pixel_index = offset_pixel.y * resolution.x + offset_pixel.x;
            Reservior prev_reservior     = u_prev_frame_reservior[offset_pixel_index];
            combined_reservior.update(prev_reservior.m_y,
                                      prev_reservior.m_p_y,
                                      prev_reservior.m_w_sum,
                                      prev_reservior.m_num_samples,
                                      payload.m_rng.next_float(rng_inc));
        }
    combined_reservior.m_inv_pdf =
        combined_reservior.m_w_sum > 0.0f
            ? combined_reservior.m_w_sum / (combined_reservior.m_p_y * combined_reservior.m_num_samples)
            : 0.0f;

    if (true)
    {
        nee = payload.m_importance * combined_reservior.m_y * combined_reservior.m_inv_pdf;
    }
    else
    {
        // check if reservior from the previous frame is valid or not
        Reservior prev_reservior = u_prev_frame_reservior[pixel_index];
        nee = payload.m_importance * prev_reservior.m_y * prev_reservior.m_inv_pdf;
    }

    payload.m_importance *= diffuse_reflectance / M_PI;
    payload.m_inout_dir = onb.to_global(local_outgoing_dir);
    payload.m_hit_pos   = vattrib.m_position;

    payload.m_radiance += nee * diffuse_reflectance + diffuse_reflectance * 0.1f;

    u_frame_reservior[pixel_index] = reservior;
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
