#version 460
#extension GL_NV_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "renderer/primpath/hitpayload.glsl"
#include "common/material.glsl"
#include "common/raycommon.glsl"
#include "shared.glsl.h"

layout(location = RAY_PRD_LOCATION) rayPayloadInNV PrimitivePathTracerPayload prd;
hitAttributeNV vec3 attribs;

layout(set = 1, binding = 0) buffer Face
{
    uint faces[];
} faces_arrays[];

layout(set = 2, binding = 0) buffer PuArray
{
    vec4 position_and_us[];
} pus_arrays[];

layout(set = 3, binding = 0) buffer NvArray
{
    vec4 normal_and_vs[];
} nvs_arrays[];

layout(set = 4, binding = 0) buffer MaterialId
{
    uint material_ids[];
} material_ids_arrays[];

layout(set = 5, binding = 0) buffer MaterialBuffer
{
    Material materials[];
} mat;

layout(set = 6, binding = 0) uniform sampler2D textures[];

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
    const vec4 nv0 = nvs_arrays[gl_InstanceID].normal_and_vs[index0];
    const vec4 nv1 = nvs_arrays[gl_InstanceID].normal_and_vs[index1];
    const vec4 nv2 = nvs_arrays[gl_InstanceID].normal_and_vs[index2];
    const vec4 pu0 = pus_arrays[gl_InstanceID].position_and_us[index0];
    const vec4 pu1 = pus_arrays[gl_InstanceID].position_and_us[index1];
    const vec4 pu2 = pus_arrays[gl_InstanceID].position_and_us[index2];
    const vec4 nv = mix_barycoord(attribs.xy, nv0, nv1, nv2);
    const vec4 pu = mix_barycoord(attribs.xy, pu0, pu1, pu2);

    // organize the values
    const vec3 normal = normalize(nv.xyz);
    const vec3 position = pu.xyz;
    const vec2 texcoord = vec2(pu.w, 1.0 - nv.w);

    const uint material_id = material_ids_arrays[gl_InstanceID].material_ids[gl_PrimitiveID];
    Material material = mat.materials[material_id];
    //material.m_diffuse_refl = mat[0].materials[material_id].xyz;

    DECODE_MATERIAL(material, textures, texcoord);

    prd.m_t = gl_HitTNV;
    prd.m_material = material;
    prd.m_normal = normal;
}
