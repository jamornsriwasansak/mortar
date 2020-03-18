#version 460
#extension GL_NV_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable

#include "shared_rt_stage/hitpayload.glsl"
#include "common/traceray.glsl"

layout(location = RAY_PRD_LOCATION) rayPayloadInNV HitPayload prd;

void main()
{
	prd.m_t = -1.0f;
}