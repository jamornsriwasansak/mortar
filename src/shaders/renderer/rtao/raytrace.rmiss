#version 460
#extension GL_NV_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable

#include "renderer/rtao/hitpayload.glsl"
#include "common/raycommon.glsl"

layout(location = RAY_PRD_LOCATION) rayPayloadInNV HitPayload prd;

void main()
{
	// detect miss if t is less < 0
	prd.m_t = -1.0f;
}