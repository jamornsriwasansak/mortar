#include "cpp_compatible.h"
#include "gbuffer_generate_ray_trace_params.h"

RAY_GEN_SHADER
void
RayGen()
{
    uint2 pixel_pos  = DispatchRaysIndex().xy;
    uint2 resolution = DispatchRaysDimensions().xy;

    const float2 uv  = (float2(pixel_pos) + 0.5f.xx) / float2(resolution);
    const float2 ndc = uv * 2.0f - 1.0f;

    const float3 origin = mul(u_cbparams.m_camera_inv_view, float4(0.0f, 0.0f, 0.0f, 1.0f)).xyz;
    const float3 lookat = mul(u_cbparams.m_camera_inv_proj, float4(ndc.x, ndc.y, 1.0f, 1.0f)).xyz;
    const float3 direction = normalize(mul(u_cbparams.m_camera_inv_view, float4(lookat, 0.0f)).xyz);

    GbufferGenerationRayTracePayload payload;
    payload.m_pixel_pos = pixel_pos;

    // Setup the ray
    RayDesc ray;
    ray.Origin    = origin;
    ray.Direction = direction;
    ray.TMin      = 0.1f;
    ray.TMax      = 100000.f;

    // Trace the ray
    u_gbuffer_depth[pixel_pos] = 0.0f;
    TraceRay(u_scene_bvh, RAY_FLAG_FORCE_OPAQUE, 0xFF, 0, 0, 0, ray, payload);
}

CLOSEST_HIT_SHADER
void
ClosestHit(INOUT(GbufferGenerationRayTracePayload) payload, const GbufferGenerationRayTraceAttributes attrib)
{
    const float2 barycentric = attrib.uv;

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
    const float3 snormal  = normalize(snormal0 * (1.0f - barycentric.x - barycentric.y) +
                                     snormal1 * barycentric.x + snormal2 * barycentric.y);

    // Interpolate Texcoord
    const float2 texcoord0 = cv0.m_texcoord;
    const float2 texcoord1 = cv1.m_texcoord;
    const float2 texcoord2 = cv2.m_texcoord;
    const float2 texcoord  = texcoord0 * (1.0f - barycentric.x - barycentric.y) +
                            texcoord1 * barycentric.x + texcoord2 * barycentric.y;

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

    const int2 pixel_pos = payload.m_pixel_pos;

    // Depth
    u_gbuffer_depth[pixel_pos]                = RayTCurrent();
    u_gbuffer_shading_normal[pixel_pos]       = snormal;
    u_gbuffer_diffuse_reflectance[pixel_pos]  = diffuse_reflectance;
    u_gbuffer_specular_reflectance[pixel_pos] = specular_reflectance;
    u_gbuffer_roughness[pixel_pos]            = roughness;
}

MISS_SHADER
void Miss(INOUT(GbufferGenerationRayTracePayload) payload) { payload.m_pixel_pos.x = -1; }
