#extension GL_GOOGLE_include_directive : enable
#include "constant.glsl"

vec3
cosine_hemisphere_from_square(const vec2 samples)
{
	const float sin_phi = sqrt(1.0f - samples[0]);
	const float theta = M_PI * 2.0f * samples[1]; // theta
	const float sin_theta = sin(theta);
	const float cos_theta = cos(theta);
	return vec3(cos_theta * sin_phi, sqrt(samples[0]), sin_theta * sin_phi);
}

vec2
latlong_texcoord_from_direction(const vec3 dir)
{
	const float u = atan(-dir[2], -dir[0]) * M_1_PI * 0.5f + 0.5f;
	const float v = acos(dir[1]) * M_1_PI;
	return vec2(u, v);
}