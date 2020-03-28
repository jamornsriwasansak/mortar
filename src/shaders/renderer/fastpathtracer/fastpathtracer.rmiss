#version 460
#extension GL_NV_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable

#include "common/traceray.glsl"
#include "renderer/fastpathtracer/fastpathtracerpayload.glsl"

layout(location = RAY_PRD_LOCATION) rayPayloadInNV FastPathTracerRayPayload prd;

void main()
{
	prd.m_t = -1.0f;
}