#include "common/mapping.h"
#include "common/onb.h"
#include "cpp_compatible.h"
#include "path_tracing_params.h"
#include "rng/pcg.h"

RAY_GEN_SHADER
void
RayGen()
{
    const uint2 pixel_pos   = DispatchRaysIndex().xy;
    const uint2 resolution  = DispatchRaysDimensions().xy;
    const uint  pixel_index = pixel_pos.y * resolution.x + pixel_pos.x;

    const float2 center_uv        = (float2(pixel_pos) + 0.5f.xx) / float2(resolution);
    const float2 center_ndc_snorm = center_uv * 2.0f - 1.0f;

    // Camera parameters
    const float3 origin = mul(u_params.m_camera_inv_view, float4(0.0f, 0.0f, 0.0f, 1.0f)).xyz;
    const float3 lookat = mul(u_params.m_camera_inv_proj, float4(center_ndc_snorm, 1.0f, 1.0f)).xyz;
    const float3 next_dir = normalize(mul(u_params.m_camera_inv_view, float4(lookat, 0.0f)).xyz);

    // Random number generator
    PcgRng rng;
    rng.init(0, pixel_index);

    // Setup ray
    RayDesc ray;
    ray.Origin    = origin;
    ray.Direction = next_dir;
    ray.TMin      = 0.1f;
    ray.TMax      = 100000.0f;

    // Setup payload
    PathTracingPayload payload;
    payload.m_is_first_bounce = true;
    payload.m_is_last_bounce  = false;
    payload.m_rnd2            = rng.next_float2();
    payload.m_miss            = false;

    // Trace Ray
    TraceRay(u_scene_bvh, RAY_FLAG_FORCE_OPAQUE, 0xff, 0, 0, 0, ray, payload);

    // Return if payload miss
    if (payload.m_miss) return;

    PathTracingShadowRayPayload shadow_payload;
    shadow_payload.m_hit = true;

    RayDesc shadow_ray;
    shadow_ray.Origin    = payload.m_t * next_dir + origin;
    shadow_ray.Direction = payload.m_next_dir;
    shadow_ray.TMin      = 0.1f;
    shadow_ray.TMax      = 100000.0f;

    // Trace Ray
    TraceRay(u_scene_bvh,
             RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER,
             0xff,
             0,
             0,
             1,
             shadow_ray,
             shadow_payload);

    if (shadow_payload.m_hit)
    {
        u_demodulated_diffuse_gi[pixel_pos] = 0.0f;
    }
    else
    {
        u_demodulated_diffuse_gi[pixel_pos] = payload.m_next_dir;
    }
}

CLOSEST_HIT_SHADER
void
ClosestHit(INOUT(PathTracingPayload) payload, const PathTracingAttributes attributes)
{
    const float2 barycentric = attributes.uv;

    uint geometry_table_index_base = 0;
    if (InstanceID() != 0)
    {
        geometry_table_index_base = u_base_instance_table[InstanceID()].m_geometry_table_index_base;
    }
    const uint               geometry_offset = geometry_table_index_base + GeometryIndex();
    const GeometryTableEntry geometry_entry  = u_geometry_table[geometry_offset];

    // Index into subbuffer
    const uint index0 = u_indices[PrimitiveIndex() * 3 + geometry_entry.m_index_base_idx];
    const uint index1 = u_indices[PrimitiveIndex() * 3 + geometry_entry.m_index_base_idx + 1];
    const uint index2 = u_indices[PrimitiveIndex() * 3 + geometry_entry.m_index_base_idx + 2];

    // Shading Normal & Texcoord
    const CompactVertex cv0 = u_compact_vertices[index0 + geometry_entry.m_vertex_base_idx];
    const CompactVertex cv1 = u_compact_vertices[index1 + geometry_entry.m_vertex_base_idx];
    const CompactVertex cv2 = u_compact_vertices[index2 + geometry_entry.m_vertex_base_idx];

    // Interpolate Shading Normal
    const float3 snormal0 = cv0.get_snormal();
    const float3 snormal1 = cv1.get_snormal();
    const float3 snormal2 = cv2.get_snormal();
    float3       snormal  = normalize(snormal0 * (1.0f - barycentric.x - barycentric.y) +
                               snormal1 * barycentric.x + snormal2 * barycentric.y);

    // Interpolate Texcoord
    const float2 texcoord0 = cv0.m_texcoord;
    const float2 texcoord1 = cv1.m_texcoord;
    const float2 texcoord2 = cv2.m_texcoord;
    const float2 texcoord  = texcoord0 * (1.0f - barycentric.x - barycentric.y) +
                            texcoord1 * barycentric.x + texcoord2 * barycentric.y;

    // TODO:: Add material graph evaluation here

    // Standard Material
    const StandardMaterial mat = u_materials[geometry_entry.m_material_idx];

    // Material Reflectance / Roughness
    const float3 diffuse_reflectance =
        mat.has_diffuse_texture()
            ? u_textures[mat.m_diffuse_tex_id].SampleLevel(u_sampler, texcoord, 0).rgb
            : mat.decode_rgb(mat.m_diffuse_tex_id);
    const float3 specular_reflectance =
        mat.has_specular_texture()
            ? u_textures[mat.m_specular_tex_id].SampleLevel(u_sampler, texcoord, 0).rgb
            : mat.decode_rgb(mat.m_specular_tex_id);
    const float roughness =
        mat.has_roughness_texture()
            ? u_textures[mat.m_roughness_tex_id].SampleLevel(u_sampler, texcoord, 0).r
            : mat.decode_rgb(mat.m_roughness_tex_id).r;

    if (payload.m_is_first_bounce)
    {
        const uint2 pixel_pos                     = DispatchRaysIndex().xy;
        u_gbuffer_depth[pixel_pos]                = RayTCurrent();
        u_gbuffer_shading_normal[pixel_pos]       = snormal;
        u_gbuffer_diffuse_reflectance[pixel_pos]  = diffuse_reflectance;
        u_gbuffer_specular_reflectance[pixel_pos] = specular_reflectance;
        u_gbuffer_roughness[pixel_pos]            = roughness;
    }

    // Construct Orthonormal basis
    const float3 dir_to_hit_pos = WorldRayDirection();
    snormal                     = faceforward(snormal, dir_to_hit_pos, snormal);
    Onb snormal_onb             = Onb_create(snormal);

    // Sample the next direction
    payload.m_next_dir = snormal_onb.to_global(cosine_hemisphere_from_square(payload.m_rnd2));
    payload.m_t        = RayTCurrent();
}

MISS_SHADER
void Miss(INOUT(PathTracingPayload) payload) { payload.m_miss = true; }

MISS_SHADER
void ShadowMiss(INOUT(PathTracingShadowRayPayload) payload) { payload.m_hit = false; }