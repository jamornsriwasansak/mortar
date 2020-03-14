#version 460
#extension GL_NV_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable

#include "renderer/primpath/hitpayload.glsl"
#include "common/raycommon.glsl"

layout(location = RAY_PRD_LOCATION) rayPayloadInNV PrimitivePathTracerPayload prd;

void main()
{
	prd.m_t = -1.0f;
}