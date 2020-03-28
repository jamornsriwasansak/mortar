#version 460
#extension GL_NV_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable

#include "renderer/fastpathtracer/fastpathtracerpayload.glsl"
#include "common/traceray.glsl"

layout(location = SHADOW_RAY_PRD_LOCATION) rayPayloadInNV FastPathTracerShadowRayPayload prd;

void main()
{
	prd.m_visibility = 1.0f;
}
