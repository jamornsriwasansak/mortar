#version 460
#extension GL_NV_ray_tracing : require

#extension GL_GOOGLE_include_directive : enable

#include "shared_rt_stage/hitpayload.glsl"
#include "common/traceray.glsl"

layout(location = SHADOW_RAY_PRD_LOCATION) rayPayloadInNV bool is_intersect;

void main()
{
	is_intersect = false;
}
