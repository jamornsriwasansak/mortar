#version 460
#extension GL_NV_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "renderer/rtao/hitpayload.glsl"
#include "common/traceray.glsl"

layout(location = RAY_PRD_LOCATION) rayPayloadInNV HitPayload prd;
hitAttributeNV vec3 attribs;

layout(set = 1, binding = 0) buffer Face
{
   uint faces[];
} faces_arrays[];

layout(set = 2, binding = 0) buffer NvArray
{
   vec4 normal_and_vs[];
} nvs_arrays[];

vec4 mix_barycoord(const vec2 bary, const vec4 a, const vec4 b, const vec4 c)
{
    return (1.0 - bary.x - bary.y) * a + bary.x * b + bary.y * c;
}

void main()
{
    const uint index0 = faces_arrays[gl_InstanceID].faces[gl_PrimitiveID * 3 + 0];
    const uint index1 = faces_arrays[gl_InstanceID].faces[gl_PrimitiveID * 3 + 1];
    const uint index2 = faces_arrays[gl_InstanceID].faces[gl_PrimitiveID * 3 + 2];

    const vec4 nv0 = nvs_arrays[gl_InstanceID].normal_and_vs[index0];
    const vec4 nv1 = nvs_arrays[gl_InstanceID].normal_and_vs[index1];
    const vec4 nv2 = nvs_arrays[gl_InstanceID].normal_and_vs[index2];

    const vec3 normal = normalize(mix_barycoord(attribs.xy, nv0, nv1, nv2).xyz);

    prd.m_t = gl_HitTNV;
    prd.m_normal = normal;
}
