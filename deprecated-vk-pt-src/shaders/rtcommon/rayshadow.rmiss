#version 460
#extension GL_NV_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable

#include "common/ray.glsl"
#include "rtcommon/raypayload.glsl"
#include "rtcommon/traceray.glsl"

layout(location = SHADOW_RAY_PRD_LOCATION) rayPayloadInNV CommonShadowRayPayload prd;

void main()
{
	prd.m_visibility = 1.0f;
}
