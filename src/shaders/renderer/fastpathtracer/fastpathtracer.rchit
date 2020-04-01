#version 460
#extension GL_NV_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "common/ray.glsl"
#include "renderer/fastpathtracer/fastpathtracerpayload.glsl"

layout(location = RAY_PRD_LOCATION) rayPayloadInNV FastPathTracerRayPayload prd;
hitAttributeNV vec3 attribs;

void main()
{
    prd.m_t = gl_HitTNV;
    prd.m_instance_id = gl_InstanceID;
    prd.m_primitive_id = gl_PrimitiveID;
    prd.m_barycoord = attribs.xy;
}
