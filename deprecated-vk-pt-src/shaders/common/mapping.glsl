#ifndef MAPPING_GLSL
#define MAPPING_GLSL

#extension GL_GOOGLE_include_directive : enable
#include "constant.glsl"

vec2
triangle_from_square(const vec2 samples)
{
#if 1
	// A Low - Distortion Map Between Triangle and Square, Eric Heitz
	if (samples.y > samples.x)
	{
		float x = samples.x * 0.5f;
		float y = samples.y - x;
		return vec2(x, y);
	}
	else
	{
		float y = samples.y * 0.5f;
		float x = samples.x - y;
		return vec2(x, y);
	}
#else
	// a well known sqrt mapping for uniformly sample a triangle
	float sqrt_sample0 = sqrt(samples.x);
	float b1 = sqrt_sample0 * (1.0f - samples.y);
	float b2 = samples.y * sqrt_sample0;
	return vec2(b1, b2);
#endif
}

vec3
cosine_hemisphere_from_square(const vec2 samples)
{
	const float sin_phi = sqrt(1.0f - samples[0]);
	const float theta = M_PI * 2.0f * samples[1]; // theta
	const float sin_theta = sin(theta);
	const float cos_theta = cos(theta);
	return vec3(cos_theta * sin_phi,
				sqrt(samples[0]),
				sin_theta * sin_phi);
}

vec3
sphere_from_square(const vec2 samples)
{
	const float z = 1.0f - 2.0f * samples.y;
	const float r = sqrt(samples.y * (1.0f - samples.y));
	const float phi = 2.0f * M_PI * samples.x; // theta = [0, 2pi)
	const float cosphi = cos(phi);
	const float sinphi = sin(phi);
	return vec3(2.0f * cosphi * r, 2.0f * sinphi * r, z);
}

vec2
panorama_from_world(const vec3 dir)
{
	const float u = atan(-dir[2], -dir[0]) * M_1_PI * 0.5f + 0.5f;
	const float v = acos(dir[1]) * M_1_PI;
	return vec2(u, v);
}

vec3
world_from_spherical(const vec2 theta_phi)
{
	const float theta = theta_phi.x;
	const float phi = theta_phi.y;
	const float sin_phi = sin(phi);
	const float cos_phi = cos(phi);
	const float sin_theta = sin(theta);
	const float cos_theta = cos(theta);
	return vec3(cos_theta * sin_phi,
				cos_phi,
				sin_theta * sin_phi);
}

vec3
world_from_panorama(const vec2 uv)
{
	const float theta = uv[0] * M_PI * 2.0f;
	const float phi = uv[1] * M_PI;
	return world_from_spherical(vec2(theta, phi));
}
#endif