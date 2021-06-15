#ifndef MICROFACET_GLSL
#define MICROFACET_GLSL

#include "common/math.glsl"

// ggx lambda function
float
Microfacet_lambda(const vec3 w, const vec2 alpha)
{
	vec2 numerator_comps = alpha * w.xz;
	float numerator = dot(numerator_comps, numerator_comps);
	float denominator = w.y * w.y;
	float sqrt_term = sqrt(1.0f + numerator / denominator);
	return (-1.0f + sqrt_term) * 0.5f;
}

float
Microfacet_g1(const vec3 w, const vec2 alpha)
{
	return 1.0f / (1.0f + Microfacet_lambda(w, alpha));
}

// G term - separable masking shadowing function 
float
Microfacet_g2(const vec3 v, const vec3 l, const vec2 alpha)
{
	// Separable Masking and Shadowing
	// Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs - equation 98
	return Microfacet_g1(v, alpha) * Microfacet_g1(l, alpha);
}

// D term - GGX distribution
float
Microfacet_d(const vec3 h,
			 const vec2 alpha)
{
	const float term = sqr(h.x / alpha.x) + sqr(h.z / alpha.y) + sqr(h.y);
	const float denominator = alpha.x * alpha.y * sqr(term);
	return M_1_PI / denominator;
}

// F term - shlick fresnel (for ggx_reflect)
vec3
Microfacet_f_shlick(const vec3 f0,
					const vec3 incoming)
{
	const float one_minus_cos = 1.0f - abs(incoming.y);
	const float exponential = sqr(sqr(one_minus_cos)) * one_minus_cos;
	return mix(vec3(exponential), vec3(1.0f), f0);
}
#endif
