#extension GL_GOOGLE_include_directive : enable
#include "shared.glsl.h"
#include "mapping.glsl"

// note all materials here are "two-sided"
// diffuse and Ggx_brdf share the same material regardless of the side of the wall.
// Ggx_btdf, on the other hand, will behave based on the incoming direction

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
	// flip outgoing based on incoming direction
	outgoing.y *= sign(incoming.y);
	bsdf_cos_contrib = material.m_diffuse_refl;
	return true;
}

bool
Diffuse_eval(out vec3 bsdf_val,
			 out float pdf,
			 const Material material,
			 const vec3 incoming,
			 const vec3 outgoing)
{
	if (incoming.y * outgoing.y <= 0.0f) return false;
	bsdf_val = material.m_diffuse_refl * M_1_PI;
	pdf = abs(outgoing.y) * M_1_PI;
	return true;
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
// TODO:: change this to fresnel term for conductor
vec3
Microfacet_f_shlick(const vec3 f0,
					const vec3 incoming)
{
	const float one_minus_cos = 1.0f - abs(incoming.y);
	const float exponential = sqr(sqr(one_minus_cos)) * one_minus_cos;
	return mix(vec3(exponential), vec3(1.0f), f0);
}

bool
Ggx_reflect_cos_sample(out vec3 outgoing,
					   out vec3 bsdf_cos_contrib,
					   const Material material,
					   const vec3 incoming,
					   const vec2 samples)
{
	const vec2 roughness = vec2(material.m_roughness);
	const vec2 alpha = sqr(roughness);

	vec3 h;
	const bool sample_success = Microfacet_sampleGGXVNDF(h,
														 incoming * sign(incoming.y),
														 alpha,
														 samples.x,
														 samples.y);
	h *= sign(incoming.y);

	if (sample_success)
	{
		outgoing = reflect(-incoming, h);
		// f_r						= f(i, h) * d(h) * g_2(i, o) / (4 * dot(i, n) * dot(o, n))
		// f_r * dot(o, n)			= f(i, h) * d(h) * g_1(i) * g_1(o) / (4 * dot(i, n))
		// pdf						= 			d(h) * g_1(i)		   /      dot(i, n)  * jacobian
		// pdf						=			d(h) * g_1(i)          / (4 * dot(i, n))
		// f_r * dot(o, n) / pdf	= f(i, h) 				  * g_1(o)
		// where refoection jacobian = 1 / 4 * dot(i, n)
		const vec3 f_term = Microfacet_f_shlick(material.m_spec_refl, incoming);
		const float g_term = Microfacet_g1(outgoing, alpha);
		bsdf_cos_contrib = f_term * g_term;
	}
	return sample_success && (outgoing.y * incoming.y > 0.0f);
}

bool
Ggx_reflect_eval(out vec3 bsdf_val,
				 out float pdf,
				 const Material material,
				 const vec3 incoming,
				 const vec3 outgoing)
{
	// make sure we are evaluating the same side of hemisphere
	if (outgoing.y * incoming.y <= 0.0f) { return false; }

	const vec2 roughness = vec2(material.m_roughness);
	const vec2 alpha = sqr(roughness);
	const vec3 h = normalize(incoming + outgoing);

	// is rare but happens
	if (any(isnan(h))) { return false; }

	const float d_term = Microfacet_d(h, alpha);
	const float g1_i_term = Microfacet_g1(incoming, alpha);
	const float g1_o_term = Microfacet_g1(outgoing, alpha);
	const float g_term = g1_i_term * g1_o_term;
	const vec3 f_term = Microfacet_f_shlick(material.m_spec_refl, incoming);

	// brdf = G * D * F / (4 * dot(v, n) * dot(l, n))
	bsdf_val = g_term * d_term * f_term / (4.0f * incoming.y * outgoing.y);

	// pdf = d(h) * g_1(v) * jacobian
	// where reflection jacobian = 1 / 4 * dot(v, n)
	const float jacobian = 1.0f / (4.0f * abs(incoming.y));
	pdf = d_term * g1_i_term * jacobian;

	return true;
}

float
Dielectric_fresnel(const float costhetai,
				   const float costhetao,
				   const float eta)
{
	float rhos = (costhetai - eta * costhetao) / (costhetai + eta * costhetao);
	float rhot = (costhetao - eta * costhetai) / (costhetao + eta * costhetai);
	return 0.5f * (rhos * rhos + rhot * rhot);
}

// note: we borrow "future" samples to decide fresnel event
bool
Ggx_transmit_cos_sample(out vec3 outgoing,
						out vec3 bsdf_cos_contrib,
						const Material material,
						const vec3 incoming,
						const vec2 samples,
						inout float next_sample)
{
	const vec2 roughness = vec2(material.m_roughness);
	const vec2 alpha = sqr(roughness);

	// sample half angle
	vec3 h;
	const bool sample_success = Microfacet_sampleGGXVNDF(h,
														 incoming * sign(incoming.y),
														 alpha,
														 samples.x,
														 samples.y);
	h *= sign(incoming.y);

	// check whether it's entering or leaving
	if (sample_success)
	{
		if (h.y * incoming.y < 0.0f) { return false; }

		const float eta = incoming.y > 0 ? material.m_ior : 1.0f / material.m_ior;
		outgoing = refract(normalize(-incoming), h, eta);
		bsdf_cos_contrib = material.m_spec_trans;

		const float f_term = clamp(Dielectric_fresnel(abs(incoming.y), abs(outgoing.y), eta), 0.0f, 1.0f);

		// as we noticed the only difference eval between reflection and refraction is fresnel term

		if (next_sample <= f_term)
		{
			// f_r						= f(i, h) * d(h) * g_2(i, o) / (4 * dot(i, n) * dot(o, n))
			// f_r * dot(o, n)			= f(i, h) * d(h) * g_1(i) * g_1(o) / (4 * dot(i, n))
			// pdf						= 			d(h) * g_1(i)		   /      dot(i, n)  * jacobian
			// where refoection jacobian = 1 / 4 * dot(i, n)
			// pdf						=			d(h) * g_1(i)          / (4 * dot(i, n))
			// f_r * dot(o, n) / pdf	= f(i, h) 				  * g_1(o)

			next_sample = next_sample / f_term;
			outgoing = reflect(-incoming, h);
			bsdf_cos_contrib *= f_term;
		}
		else
		{
			// f_t						= dot(i, h) * dot(o, h) * eta^2 * (1 - f(i, h)) * d(h) * g_1(i) * g_1(o) / (dot(i, n) * dot(o, n) * (dot(i, h) + eta * dot(o, h))^2)
			// f_t * dot(o, n)			= dot(i, h) * dot(o, h) * eta^2 * (1 - f(i, h)) * d(h) * g_1(i) * g_1(o) / (dot(i, n) * (dot(i, h) + eta * dot(o, h))^2)
			// pdf						= dot(i, h)										* d(h) * g_1(i)	* jacobian													/ dot(i ,n)
			// where jacobian(dh / di)	=			  dot(o, h) * eta^2											 / (dot(i, n) * (dot(i, h) + eta * dot(o, h))^2)
			// pdf						= dot(i, h) * dot(o, h)	* eta^2					* d(h) * g_1(i)			 / (dot(i, n) * (dot(i, h) + eta * dot(o, h))^2)
			// and since dot(i, h) = dot(o, h)
			// f_t * dot(o, n) / pdf	=								  (1 - f(i, h))					* g_1(o)

			next_sample = (next_sample - f_term) / (1.0f - f_term);
			bsdf_cos_contrib *= 1.0f - f_term;
		}

		const float g_term = Microfacet_g1(outgoing, alpha);
		bsdf_cos_contrib *= g_term;
	}

	return sample_success;
}

bool
Ggx_transmit_eval(out vec3 bsdf_val,
				  out float pdf,
				  const Material material,
				  const vec3 incoming,
				  const vec3 outgoing)
{
	// check whether it's entering or leaving
	const vec2 roughness = vec2(material.m_roughness);
	const vec2 alpha = sqr(roughness);
	const float eta = incoming.y > 0 ? material.m_ior : 1.0f / material.m_ior;
	vec3 h = normalize(incoming + eta * outgoing);
	
	// is rare but happens
	if (any(isnan(h))) { return false; }

	const float d_term = Microfacet_d(h, alpha);
	const float g1_i_term = Microfacet_g1(incoming, alpha);
	const float g1_o_term = Microfacet_g1(outgoing, alpha);
	const float g_term = g1_i_term * g1_o_term;
	const float f_term = clamp(Dielectric_fresnel(abs(incoming.y), abs(outgoing.y), eta), 0.0f, 1.0f);

	bsdf_val = material.m_spec_trans;

	if (incoming.y * outgoing.y > 0.0f)
	{
		// same side: must be reflection
		// f_r						= f(i, h) * d(h) * g_1(i) * g_1(o) / (4 * dot(i, n) * dot(o, n))
		// pdf						= f(i, h) * d(h) * g_1(i)          / (4 * dot(i, n))
		if (d_term == 0.0f)
		{
			return false;
		}
		bsdf_val *= f_term * d_term * g1_i_term * g1_o_term / (4.0f * abs(incoming.y) * abs(outgoing.y));
		pdf = f_term * d_term * g1_i_term / (4.0f * abs(incoming.y));
	}
	else
	{
		const float dot_i_h = abs(dot(incoming, h));
		const float dot_o_h = abs(dot(outgoing, h));
		// different side: must be refraction
		// f_t						= dot(i, h) * dot(o, h) * eta^2 * (1 - f(i, h)) * d(h) * g_1(i) * g_1(o) / (dot(i, n) * dot(o, n) * (dot(i, h) + eta * dot(o, h))^2)
		// pdf						= dot(i, h) * dot(o, h)	* eta^2					* d(h) * g_1(i)			 / (dot(i, n) * (dot(i, h) + eta * dot(o, h))^2)
		bsdf_val *= dot_i_h * dot_o_h * sqr(eta) * (1.0f - f_term) * d_term * g1_i_term * g1_o_term / (abs(incoming.y) * abs(outgoing.y) * sqr(dot_i_h + eta * dot_o_h));
		pdf = dot_i_h * dot_o_h * sqr(eta) * d_term * g1_i_term / (abs(incoming.y) * sqr(dot_i_h + eta * dot_o_h));
	}

	return true;
}

//
// Mixed material
//

bool
Material_cos_sample(out vec3 outgoing,
					out vec3 bsdf_cos_contrib,
					inout float ior,
					const Material material,
					const vec3 incoming,
					vec2 samples,
					inout float next_sample)
{
	const float diffuse_weight = length(material.m_diffuse_refl);
	const float ggx_refl_weight = length(material.m_spec_refl);
	const float ggx_trans_weight = length(material.m_spec_trans);
	const float sum_weight = diffuse_weight + ggx_refl_weight + ggx_trans_weight;

	// break if both weight are too low.
	// probably a light source or a completely dark material.
	if (sum_weight <= 1e-5f) { return false; }

	// probability of choosing each material
	const float diffuse_cprob = diffuse_weight / sum_weight;
	const float ggx_refl_cprob = ggx_refl_weight / sum_weight;
	const float ggx_trans_cprob = ggx_trans_weight / sum_weight;

	if (samples.x < diffuse_cprob) // r is in between [0, diffuse_cprob]
	{
		// scale sample
		samples.x = samples.x / diffuse_cprob;

		// sample using diffuse
		vec3 diffuse_bsdf_contrib;
		const bool sample_success = Diffuse_cos_sample(outgoing, diffuse_bsdf_contrib, material, incoming, samples);
		if (!sample_success) { return false; }
	}
	else if (samples.x < diffuse_cprob + ggx_refl_cprob) // r is in between [diffuse_cprob, diffuse_cprob + ggx_refl_cprob]
	{
		// scale sample
		samples.x = (samples.x - diffuse_cprob) / ggx_refl_cprob;

		// sample using ggx reflection
		vec3 ggx_refl_bsdf_contrib;
		const bool sample_success = Ggx_reflect_cos_sample(outgoing, ggx_refl_bsdf_contrib, material, incoming, samples);
		if (!sample_success) { return false; }
	}
	else // r is in between [diffuse_cprob + ggx_refl_cprob, 1.0]
	{
		// scale sample
		samples.x = (samples.x - (diffuse_cprob + ggx_refl_cprob)) / ggx_trans_cprob;

		// sample using ggx transmission
		vec3 ggx_trans_bsdf_contrib;
		const bool sample_success = Ggx_transmit_cos_sample(outgoing, ggx_trans_bsdf_contrib, material, incoming, samples, next_sample);
		if (!sample_success) { return false; }
	}

	// note: SUM(f_i(v, l)) / SUM(p_i(v, l)) corresponded to applying MIS on top of bsdf sampling techniques
	// thus, reduces level of noise.
	vec3 bsdf_cos_val_sum = vec3(0.0f);
	float bsdf_pdf_sum = 0.0f;

	// eval diffuse
	if (diffuse_cprob > 0.0f)
	{
		vec3 diffuse_bsdf_val;
		float diffuse_pdf;
		if (Diffuse_eval(diffuse_bsdf_val, diffuse_pdf, material, incoming, outgoing))
		{
			bsdf_cos_val_sum += diffuse_bsdf_val;
			bsdf_pdf_sum += diffuse_pdf * diffuse_cprob;
		}
	}

	// eval ggx reflection
	if (ggx_refl_cprob > 0.0f)
	{
		vec3 ggx_refl_bsdf_val;
		float ggx_refl_pdf;
		if (Ggx_reflect_eval(ggx_refl_bsdf_val, ggx_refl_pdf, material, incoming, outgoing))
		{
			bsdf_cos_val_sum += ggx_refl_bsdf_val;
			bsdf_pdf_sum += ggx_refl_pdf * ggx_refl_cprob;
		}
	}

	// eval ggx transmission
	if (ggx_trans_cprob > 0.0f)
	{
		vec3 ggx_trans_bsdf_val;
		float ggx_trans_pdf;
		if (Ggx_transmit_eval(ggx_trans_bsdf_val, ggx_trans_pdf, material, incoming, outgoing))
		{
			bsdf_cos_val_sum += ggx_trans_bsdf_val;
			bsdf_pdf_sum += ggx_trans_pdf * ggx_trans_cprob;
		}
	}

	// use mis to compute bsdf_weight
	bsdf_cos_contrib = bsdf_pdf_sum <= 1e-8f ? vec3(0.0f) : bsdf_cos_val_sum / bsdf_pdf_sum * abs(outgoing.y);

	return true;
}
