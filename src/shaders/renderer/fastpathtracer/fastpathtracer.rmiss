#version 460
#extension GL_NV_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable

#include "common/traceray.glsl"
#include "common/mapping.glsl"
#include "renderer/fastpathtracer/fastpathtracerpayload.glsl"

layout(location = RAY_PRD_LOCATION) rayPayloadInNV FastPathTracerRayPayload prd;
layout(set = 0, binding = 3) uniform sampler2D envmap;

void main()
{
/*
	// fetch emissive value from envmap
	const vec2 latlong_texcoord = panorama_from_world(normalize(gl_WorldRayDirectionNV));
	const vec3 emission = texture(envmap, latlong_texcoord).xyz;
	prd.m_color = prd.m_importance * emission;
	*/
	prd.m_t = -1.0f;
}