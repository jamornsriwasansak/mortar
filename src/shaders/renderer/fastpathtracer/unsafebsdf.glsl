// this is almost a duplicate of bsdf.glsl
// however, there are a few differences.
// 1. it does not prevent light leak.
// 2. it assumes that incoming and outgoing are always lie within an upper hemisphere.
// this helps removing a lot of conditions from the sourcecode (and thus less divergence)

#extension GL_GOOGLE_include_directive : enable
#include "shared.glsl.h"
#include "common/bsdf.glsl"
#include "common/mapping.glsl"
#include "common/math.glsl"

vec3
Fast_Diffuse_cos_sample(out vec3 outgoing,
						out float pdf,
						const Material material,
						const vec3 incoming,
						const vec2 samples)
{
	outgoing = cosine_hemisphere_from_square(samples);
	pdf = outgoing.y * M_1_PI;
	return material.m_diffuse_refl;
}

vec3
Fast_Diffuse_eval(out float pdf,
				  const Material material,
				  const vec3 incoming,
				  const vec3 outgoing)
{
	pdf = outgoing.y * M_1_PI;
	return material.m_diffuse_refl * M_1_PI;
}

// Code provided by heitz et al.
// http://jcgt.org/published/0007/04/01/
void
Fast_Microfacet_sampleGGXVNDF(out vec3 h,
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
}

vec3
Fast_Ggx_reflect_cos_sample(out vec3 outgoing,
							out float pdf,
							const Material material,
							const vec3 incoming,
							const vec2 samples)
{
	const vec2 roughness = vec2(material.m_roughness);
	const vec2 alpha = sqr(roughness);
	vec3 h;
	Fast_Microfacet_sampleGGXVNDF(h, incoming, alpha, samples.x, samples.y);
	outgoing = reflect(-incoming, h);

	// f_r						= f(i, h) * d(h) * g_2(i, o) / (4 * dot(i, n) * dot(o, n))
	// f_r * dot(o, n)			= f(i, h) * d(h) * g_1(i) * g_1(o)
	// pdf						= 			d(h) * g_1(i)		   * jacobian
	// pdf						=			d(h) * g_1(i)          / (4 * dot(i, n))
	// f_r * dot(o, n) / pdf	= f(i, h) 				  * g_1(o)
	// where reflection jacobian = 1 / 4 * dot(i, n)

	// compute pdf
	const float d_term = Microfacet_d(h, alpha);
	const float g1_i_term = Microfacet_g1(incoming, alpha);
	pdf = d_term * g1_i_term / (4.0f * abs(incoming.y) + SMALL_VALUE);

	// compute brdf contrib
	const vec3 f_term = material.m_spec_refl;//Microfacet_f_shlick(material.m_spec_refl, incoming);
	const float g1_o_term = Microfacet_g1(outgoing, alpha);
	return f_term * g1_o_term;
}

vec3
Fast_Ggx_reflect_eval(out float pdf,
					  const Material material,
					  const vec3 incoming,
					  const vec3 outgoing)
{
	const vec2 roughness = vec2(material.m_roughness);
	const vec2 alpha = sqr(roughness);
	const vec3 h = normalize(incoming + outgoing);

	const float d_term = Microfacet_d(h, alpha);
	const float g1_i_term = Microfacet_g1(incoming, alpha);
	const float g1_o_term = Microfacet_g1(outgoing, alpha);
	const vec3 f_term = material.m_spec_refl;// Microfacet_f_shlick(material.m_spec_refl, incoming);

	// pdf = d(h) * g_1(v) * jacobian
	// where reflection jacobian = 1 / 4 * dot(v, n)
	// pdf = d(h) * g_1(v) / 4 * dot(v, n)
	pdf = d_term * g1_i_term / (4.0f * abs(incoming.y) + SMALL_VALUE);

	// brdf = G * D * F / (4 * dot(v, n) * dot(l, n))
	//return g1_o_term * f_term * pdf / (outgoing.y + SMALL_VALUE);
	return f_term;
}

vec3
Fast_Material_cos_sample(out vec3 outgoing,
						 out float pdf,
						 const Material material,
						 const vec3 incoming,
						 vec2 samples)
{
	// compute probability of choosing a material
	const float diffuse_weight = luminance(material.m_diffuse_refl);
	const float ggx_weight = luminance(material.m_spec_refl);
	const float diffuse_cprob = diffuse_weight / (diffuse_weight + ggx_weight);
	const float ggx_cprob = 1.0f - diffuse_cprob;

	vec3 brdf_val_cos;
	float brdf_pdf;

	if (samples.x < diffuse_cprob) // r is in between [0, diffuse_cprob]
	{
		// scale sample
		samples.x = samples.x / diffuse_cprob;

		// sample using diffuse
		float diffuse_pdf;
		vec3 diffuse_brdf_cos_contrib = Fast_Diffuse_cos_sample(outgoing, diffuse_pdf, material, incoming, samples);
		vec3 diffuse_brdf_cos = diffuse_brdf_cos_contrib * diffuse_pdf;

		// evaluate ggx
		float ggx_pdf;
		vec3 ggx_brdf_cos = Fast_Ggx_reflect_eval(ggx_pdf, material, incoming, outgoing) * outgoing.y;

		brdf_val_cos = diffuse_brdf_cos + ggx_brdf_cos;
		brdf_pdf = diffuse_pdf * diffuse_cprob + ggx_pdf * ggx_cprob;
	}
	else
	{
		// scale sample
		samples.x = (samples.x - diffuse_cprob) / ggx_cprob;

		// sample using ggx reflection
		float ggx_pdf;
		vec3 ggx_brdf_cos_contrib = Fast_Ggx_reflect_cos_sample(outgoing, ggx_pdf, material, incoming, samples);
		vec3 ggx_brdf_cos = ggx_brdf_cos_contrib * ggx_pdf;

		// evaluate diffuse
		float diffuse_pdf;
		vec3 diffuse_brdf_cos = Fast_Diffuse_eval(diffuse_pdf, material, incoming, outgoing) * outgoing.y;

		brdf_val_cos = diffuse_brdf_cos + ggx_brdf_cos;
		brdf_pdf = diffuse_pdf*diffuse_cprob + ggx_pdf*ggx_cprob;
	}

	pdf = brdf_pdf;
	return brdf_val_cos / brdf_pdf;
}

vec3
Fast_Material_eval(out float pdf,
				   const Material material,
				   const vec3 incoming,
				   const vec3 outgoing)
{
	// compute probability of choosing a material
	const float diffuse_weight = luminance(material.m_diffuse_refl) + SMALL_VALUE;
	const float ggx_weight = luminance(material.m_spec_refl) + SMALL_VALUE;
	const float diffuse_cprob = diffuse_weight / (diffuse_weight + ggx_weight);
	const float ggx_cprob = 1.0f - diffuse_cprob;

	// evaluate ggx
	float ggx_pdf;
	vec3 ggx_brdf = Fast_Ggx_reflect_eval(ggx_pdf, material, incoming, outgoing);

	// evaluate diffuse
	float diffuse_pdf;
	vec3 diffuse_brdf = Fast_Diffuse_eval(diffuse_pdf, material, incoming, outgoing);

	// compute pdf
	pdf = ggx_pdf*ggx_cprob + diffuse_pdf*diffuse_cprob;

	// compute brdf
	return ggx_brdf + diffuse_brdf;
}
