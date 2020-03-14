#extension GL_GOOGLE_include_directive : enable
#include "shared.glsl.h"
#include "mapping.glsl"

vec3
Lambert_sample(const vec3 incoming,
			   const vec2 samples)
{
	return cosine_hemisphere_from_square(samples);
}

vec3
Lambert_eval(const vec3 incoming,
			 const vec3 outgoing)
{
	return vec3(1.0);
}