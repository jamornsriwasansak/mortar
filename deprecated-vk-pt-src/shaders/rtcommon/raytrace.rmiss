#version 460
#extension GL_NV_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable

#include "common/ray.glsl"
#include "rtcommon/raypayload.glsl"

layout(location = RAY_PRD_LOCATION) rayPayloadInNV CommonRayPayload prd;

void main()
{
	prd.m_t = -1.0f;
}