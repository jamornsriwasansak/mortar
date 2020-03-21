#version 460
#extension GL_NV_ray_tracing : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

#include "common/ray.glsl"
#include "shared_rt_stage/hitpayload.glsl"
#include "shared.glsl.h"

layout(location = RAY_PRD_LOCATION) rayPayloadInNV HitPayload prd;
hitAttributeNV vec3 attribs;

layout(set = 1, binding = 0) buffer Face
{
    uint faces[];
} faces_arrays[];

layout(set = 1, binding = 1) buffer PuArray
{
    vec4 position_and_us[];
} pus_arrays[];

layout(set = 1, binding = 2) buffer NvArray
{
    vec4 normal_and_vs[];
} nvs_arrays[];

layout(set = 1, binding = 3) buffer MaterialId
{
    uint material_ids[];
} material_ids_arrays[];

layout(set = 1, binding = 4) buffer MaterialBuffer
{
    Material materials[];
} mat;

layout(set = 1, binding = 5) uniform sampler2D textures[];

vec4 mix_barycoord(const vec2 bary, const vec4 a, const vec4 b, const vec4 c)
{
    return (1.0f - bary.x - bary.y) * a + bary.x * b + bary.y * c;
}

void main()
{
    // fetch from ssbo
    const uint index0 = faces_arrays[gl_InstanceID].faces[gl_PrimitiveID * 3 + 0];
    const uint index1 = faces_arrays[gl_InstanceID].faces[gl_PrimitiveID * 3 + 1];
    const uint index2 = faces_arrays[gl_InstanceID].faces[gl_PrimitiveID * 3 + 2];

    // position and tex u
    const vec4 pu0 = pus_arrays[gl_InstanceID].position_and_us[index0];
    const vec4 pu1 = pus_arrays[gl_InstanceID].position_and_us[index1];
    const vec4 pu2 = pus_arrays[gl_InstanceID].position_and_us[index2];
    const vec4 pu = mix_barycoord(attribs.xy, pu0, pu1, pu2);

    // normal and tex v
    const vec4 nv0 = nvs_arrays[gl_InstanceID].normal_and_vs[index0];
    const vec4 nv1 = nvs_arrays[gl_InstanceID].normal_and_vs[index1];
    const vec4 nv2 = nvs_arrays[gl_InstanceID].normal_and_vs[index2];
    const vec4 nv = mix_barycoord(attribs.xy, nv0, nv1, nv2);

    // correct normals and make sure normal share the same direction as incoming vector (= -ray.m_direction)
    vec3 gnormal = normalize(cross(pu0.xyz - pu1.xyz, pu0.xyz - pu2.xyz));
    vec3 snormal = normalize(nv.xyz);
    //gnormal = dot(gnormal, -gl_WorldRayDirectionNV) >= 0.0f ? gnormal : -gnormal;
    snormal = dot(gnormal, snormal) >= 0.0f ? snormal : -snormal;

    // organize the values
    const vec3 position = pu.xyz;
    const vec2 texcoord = vec2(pu.w, nv.w);

    const uint material_id = material_ids_arrays[gl_InstanceID].material_ids[gl_PrimitiveID];
    Material material = mat.materials[material_id];

    DECODE_MATERIAL(material, textures, texcoord);

    prd.m_t = gl_HitTNV;
    prd.m_material = material;
    prd.m_snormal = snormal;
    prd.m_gnormal = gnormal;
}
