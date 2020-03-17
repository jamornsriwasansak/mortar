#extension GL_GOOGLE_include_directive : enable
#include "shared.glsl.h"
#include "mapping.glsl"

float
sqr(float v)
{
	return v * v;
}

vec2
sqr(vec2 v)
{
	return v * v;
}

//
// Lambert
//

#define MAT_DIFFUSE_INDEX 0

bool
Diffuse_cos_sample(out vec3 outgoing,
				   out vec3 bsdf_cos_contrib,
				   const Material material,
				   const vec3 incoming,
				   const vec2 samples)
{
	outgoing = cosine_hemisphere_from_square(samples);
	bsdf_cos_contrib = material.m_diffuse_refl;// *step(0.0f, incoming.y);
	return true;
}

float
Diffuse_cos_sampling_pdf(const Material material,
						 const vec3 incoming,
						 const vec3 outgoing)
{
	return outgoing.y * M_1_PI;
}

vec3
Diffuse_eval(const Material material,
			 const vec3 incoming,
			 const vec3 outgoing)
{
	return material.m_diffuse_refl * M_1_PI;// *step(0.0f, incoming.y) * step(0.0f, outgoing.y);
}

//
// Trowbridge-reitz (GGX) Microfacet
//

#define MAT_GGX_INDEX 1

// Code provided by heitz et al.
// http://jcgt.org/published/0007/04/01/
bool
Microfacet_sampleGGXVNDF(out vec3 h,
						 const vec3 incoming,
						 const vec2 alpha,
						 const float U1,
						 const float U2)
{
	// Input Ve: view direction
	// Input alpha_x, alpha_y: roughness parameters
	// Input U1, U2: uniform random numbers
	// Output Ne: normal sampled with PDF D_Ve(Ne) = G1(Ve) * max(0, dot(Ve, Ne)) * D(Ne) / Ve.z

	vec3 Ve = incoming.xzy;

	// Section 3.2: transforming the view direction to the hemisphere configuration
	vec3 Vh = normalize(vec3(alpha.x * Ve.x, alpha.y * Ve.y, Ve.z));
	// Section 4.1: orthonormal basis (with special case if cross product is zero)
	float lensq = dot(Vh.xy, Vh.xy);
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
	vec3 Nh = t1 * T1 + t2 * T2 + sqrt(max(0.0, 1.0 - r*r)) * Vh;
	// Section 3.4: transforming the normal back to the ellipsoid configuration
	vec3 Ne = normalize(vec3(alpha.x * Nh.x, alpha.y * Nh.y, max(0.0f, Nh.z)));
	h = Ne.xzy;
	return !any(isnan(h));
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
Microfacet_g1(const vec3 w, const vec3 h, const vec2 alpha)
{
	if (dot(w, h) < 0.0f) { return 0.0f; }
	return 1.0f / (1.0f + Microfacet_lambda(w, alpha));
}

// G term - separable masking shadowing function 
float
Microfacet_g2(const vec3 v, const vec3 l, const vec3 h, const vec2 alpha)
{
	// Separable Masking and Shadowing
	// Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs - equation 98
	return Microfacet_g1(v, h, alpha) * Microfacet_g1(l, h, alpha);
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

bool
Ggx_cos_sample(out vec3 outgoing,
			   out vec3 bsdf_cos_contrib,
			   const Material material,
			   const vec3 incoming,
			   const vec2 samples)
{
	const vec2 roughness = vec2(material.m_roughness);
	const vec2 alpha = sqr(roughness);

	vec3 h;
	const bool sample_success = Microfacet_sampleGGXVNDF(h,
														 incoming,
														 alpha,
														 samples.x,
														 samples.y);
	if (sample_success)
	{
		outgoing = reflect(-incoming, h);
		// f_r						= f(v, h) * d(h) * g_2(v, l) / (4 * dot(v, n) * dot(l, n))
		// f_r * dot(l, n)			= f(v, h) * d(h) * g_2(v, l) / (4 * dot(v, n))
		// pdf						= d_v(v, l, h) / (4 * dot(v, h))

		// after simplification
		// f_r * dot(l, n)			= f(v, h) * d(h) * g_1(v) * g_1(l) / (4 * dot(v, n))
		// pdf						=			d(h) * g_1(v)          / (4 * dot(v, n))
		// f_r * dot(l, n) / pdf	= f(v, h) 				  * g_1(l)
		bsdf_cos_contrib = material.m_spec_refl * vec3(Microfacet_g1(outgoing, h, alpha));
	}
	return sample_success;
}

float
Ggx_cos_sampling_pdf(const Material material,
					 const vec3 incoming,
					 const vec3 outgoing)
{
	const vec2 roughness = vec2(material.m_roughness);
	const vec2 alpha = sqr(roughness);

	// pdf = d(h) * g_1(v) / (4 * dot(v, n))
	const vec3 h = normalize(incoming + outgoing);
	const float d_term = Microfacet_d(h, alpha);
	const float g1_term = Microfacet_g1(incoming, h, alpha);
	const float denominator = 4.0f * incoming.y;

	return d_term * g1_term / denominator;
}

vec3
Ggx_eval(const Material material,
		 const vec3 incoming,
		 const vec3 outgoing)
{
	const vec2 roughness = vec2(material.m_roughness);
	const vec2 alpha = sqr(roughness);
	const vec3 h = normalize(incoming + outgoing);
	
	// compute g term and d term
	const float g_term = Microfacet_g2(incoming, outgoing, h, alpha);
	const float d_term = Microfacet_d(h, alpha);

	return material.m_spec_refl * g_term * d_term / (4.0f * incoming.y * outgoing.y);
}

//
// Mixed material
//

bool
Material_cos_sample(out vec3 outgoing,
					out vec3 bsdf_cos_contrib,
					const Material material,
					const vec3 incoming,
					vec2 samples)
{
	const float diffuse_weight = length(material.m_diffuse_refl);
	const float ggx_weight = length(material.m_spec_refl);
	const float sum_weight = diffuse_weight + ggx_weight;

	// break if both weight are too low.
	// probably a light source or a completely dark material.
	if (sum_weight <= 1e-5f) { return false; }

	// probability of choosing each material
	const float diffuse_cprob = diffuse_weight / sum_weight;
	const float ggx_cprob = ggx_weight / sum_weight;

	if (samples.x < diffuse_cprob)
	{
		vec3 diffuse_bsdf_weight;

		// scale sample
		samples.x = samples.x / diffuse_cprob;

		// choose diffuse
		const bool sample_success = Diffuse_cos_sample(outgoing, diffuse_bsdf_weight, material, incoming, samples);
		if (!sample_success) { return false; }
	}
	else
	{
		vec3 ggx_bsdf_weight;

		// scale sample
		samples.x = (samples.x - diffuse_cprob) / ggx_cprob;

		// choose ggx
		const bool sample_success = Ggx_cos_sample(outgoing, ggx_bsdf_weight, material, incoming, samples);
		if (!sample_success) { return false; }
	}

	// eval both
	vec3 diffuse_bsdf_val = Diffuse_eval(material, incoming, outgoing);
	float diffuse_pdf = Diffuse_cos_sampling_pdf(material, incoming, outgoing) * diffuse_cprob;
	vec3 ggx_bsdf_val = Ggx_eval(material, incoming, outgoing);
	float ggx_pdf = Ggx_cos_sampling_pdf(material, incoming, outgoing) * ggx_cprob;

	// use mis to compute bsdf_weight
	bsdf_cos_contrib = (diffuse_bsdf_val + ggx_bsdf_val) / (diffuse_pdf + ggx_pdf) * outgoing.y;

	return true;
}
