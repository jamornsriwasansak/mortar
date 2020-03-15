#extension GL_GOOGLE_include_directive : enable
#include "shared.glsl.h"
#include "mapping.glsl"

//
// Lambert
//

void
Lambert_sample(out vec3 local_outgoing,
			   out vec3 bsdf_weight,
			   const Material material,
			   const vec3 incoming,
			   const vec2 samples)
{
	local_outgoing = cosine_hemisphere_from_square(samples);
	bsdf_weight = material.m_diffuse_refl;// *step(0.0f, incoming.y);
}

vec3
Lambert_eval(const Material material,
			 const vec3 incoming,
			 const vec3 outgoing)
{
	return material.m_diffuse_refl;// *step(0.0f, incoming.y) * step(0.0f, outgoing.y);
}

//
// GGX Microfacet
//

// Code provided by heitz et al.
// http://jcgt.org/published/0007/04/01/
vec3
Microfacet_sampleGGXVNDF(vec3 Ve, vec2 alpha, float U1, float U2)
{
	// Input Ve: view direction
	// Input alpha_x, alpha_y: roughness parameters
	// Input U1, U2: uniform random numbers
	// Output Ne: normal sampled with PDF D_Ve(Ne) = G1(Ve) * max(0, dot(Ve, Ne)) * D(Ne) / Ve.z

	// Section 3.2: transforming the view direction to the hemisphere configuration
	vec3 Vh = normalize(vec3(alpha.x * Ve.x, alpha.y * Ve.y, Ve.z));
	// Section 4.1: orthonormal basis (with special case if cross product is zero)
	float lensq = Vh.x * Vh.x + Vh.y * Vh.y;
	vec3 T1 = lensq > 0 ? vec3(-Vh.y, Vh.x, 0) * inversesqrt(lensq) : vec3(1, 0, 0);
	vec3 T2 = cross(Vh, T1);
	// Section 4.2: parameterization of the projected area
	float r = sqrt(U1);
	float phi = 2.0 * M_PI * U2;
	float t1 = r * cos(phi);
	float t2 = r * sin(phi);
	float s = 0.5 * (1.0 + Vh.z);
	t2 = (1.0 - s) * sqrt(1.0 - t1 * t1) + s * t2;
	// Section 4.3: reprojection onto hemisphere
	vec3 Nh = t1 * T1 + t2 * T2 + sqrt(max(0.0, 1.0 - t1 * t1 - t2 * t2)) * Vh;
	// Section 3.4: transforming the normal back to the ellipsoid configuration
	vec3 Ne = normalize(vec3(alpha.x * Nh.x, alpha.y * Nh.y, max(0.0f, Nh.z)));
	return Ne;
}

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

float
Microfacet_g2(const vec3 wi, const vec3 wo, const vec2 alpha)
{
	// Separable Masking and Shadowing
	// Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs - equation 98
	return Microfacet_g1(wi, alpha) * Microfacet_g1(wo, alpha);
}

void
Ggx_sample(out vec3 local_outgoing,
		   out vec3 bsdf_weight,
		   const Material material,
		   const vec3 incoming,
		   const vec2 samples)
{
	const vec2 alpha = vec2(0.1f, 0.1f);

	const vec3 h = Microfacet_sampleGGXVNDF(incoming.xzy, alpha, samples.x, samples.y).xzy;
	local_outgoing = reflect(-incoming, h);

	// f_r(v, l) = f(v, h) * d_v(v, l, h) * g_2(v, l)
	// pdf = d_v(v, l, h) / (4 * dot(v, h))
	// f_r / pdf = f(v, h) * g_2(v, l) / g_1(v) = f(v, h) * g_1(v) * g_1(l) / g_1(v) = f(v, h) * g_1(l)
	bsdf_weight = vec3(Microfacet_g1(local_outgoing, alpha));
}