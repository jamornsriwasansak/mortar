// this is almost a duplicate of bsdf.glsl
// however, there are a few differences.
// 1. it does not prevent light leak.
// 2. it assumes that incoming and outgoing are always lie within an upper hemisphere.
// this helps removing a lot of conditions from the sourcecode (and thus less divergence)

#extension GL_GOOGLE_include_directive : enable
#include "shared.glsl.h"
#include "material.glsl.h"
#include "common/microfacet.glsl"
#include "common/mapping.glsl"
#include "common/math.glsl"

vec3
Diffuse_cos_sample(out vec3 outgoing,
				   out float pdf,
				   const Material material,
				   const vec3 incoming,
				   const vec2 samples)
{
	outgoing = cosine_hemisphere_from_square(samples) * sign(incoming.y);
	pdf = outgoing.y * M_1_PI;
	return material.m_diffuse_refl;
}

vec3
Diffuse_eval(out float pdf,
			 const Material material,
			 const vec3 incoming,
			 const vec3 outgoing)
{
	const float is_same_side = step(0.0f, incoming.y * outgoing.y);
	pdf = abs(outgoing.y) * M_1_PI * is_same_side;
	return material.m_diffuse_refl * M_1_PI * is_same_side;
}

// Code provided by heitz et al.
// http://jcgt.org/published/0007/04/01/
void
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
}

vec3
Ggx_reflect_cos_sample(out vec3 outgoing,
					   out float pdf,
					   const Material material,
					   const vec3 incoming,
					   const vec2 samples)
{
	const vec2 roughness = vec2(material.m_roughness);
	const vec2 alpha = sqr(roughness);
	const float side = sign(incoming.y);
	vec3 h;
	Microfacet_sampleGGXVNDF(h, incoming * side, alpha, samples.x, samples.y);
	outgoing = reflect(-incoming, h) * side;

	// f_r							 =			  f(i, h) * d(h) * g_2(i, o)	   / (4 * dot(i, n) * dot(o, n))
	// f_r * dot(o, n)				 =			  f(i, h) * d(h) * g_1(i) * g_1(o) / (4 * dot(i, n))
	// pdf							 = dot(i, h)		  * d(h) * g_1(i)		   / (	  dot(i, n)) * jacobian
	// where reflection jacobian	 = 1										   / (4				 * dot(i, h))
	// pdf							 = 					  * d(h) * g_1(i)		   / (4 * dot(i, n))
	// f_r * dot(o, n) / pdf		 = f(i, h) 					 * g_1(o)

	const float is_same_side = step(0.0f, incoming.y * outgoing.y);

	// compute pdf
	const float d_term = Microfacet_d(h, alpha);
	const float g1_i_term = Microfacet_g1(incoming, alpha);
	pdf = d_term * g1_i_term / (4.0f * abs(incoming.y) + SMALL_VALUE) * is_same_side;

	// compute brdf contrib
	const vec3 f_term = material.m_spec_refl;//Microfacet_f_shlick(material.m_spec_refl, incoming);
	const float g1_o_term = Microfacet_g1(outgoing, alpha);
	return f_term * g1_o_term * is_same_side;
}

vec3
Ggx_reflect_eval(out float pdf,
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

	const float is_same_side = step(0.0f, incoming.y * outgoing.y);

	// pdf = d(h) * g_1(i) / 4 * dot(i, n)
	pdf = d_term * g1_i_term / (4.0f * abs(incoming.y) + SMALL_VALUE) * is_same_side;

	// brdf = G * D * F / (4 * dot(i, n) * dot(o, n))
	return g1_o_term * f_term * pdf * step(0.0f, incoming.y * outgoing.y) / (outgoing.y + SMALL_VALUE) * is_same_side;
}

vec3
Material_cos_sample(out vec3 outgoing,
					out float pdf,
					const Material material,
					const vec3 incoming,
					vec2 samples)
{
	// compute probability of choosing a material
	const float diffuse_weight = luminance(material.m_diffuse_refl);
	const float ggx_reflect_weight = luminance(material.m_spec_refl);
	const float diffuse_cprob = diffuse_weight / (diffuse_weight + ggx_reflect_weight);
	const float ggx_reflect_cprob = 1.0f - diffuse_cprob;

	vec3 brdf_val_cos;
	float brdf_pdf;

	if (samples.x < diffuse_cprob) // r is in between [0, diffuse_cprob]
	{
		// scale sample
		samples.x = samples.x / diffuse_cprob;

		// sample using diffuse
		float diffuse_pdf;
		vec3 diffuse_brdf_cos_contrib = Diffuse_cos_sample(outgoing, diffuse_pdf, material, incoming, samples);
		vec3 diffuse_brdf_cos = diffuse_brdf_cos_contrib * diffuse_pdf;

		// evaluate ggx reflection
		float ggx_reflect_pdf;
		vec3 ggx_reflect_brdf_cos = Ggx_reflect_eval(ggx_reflect_pdf, material, incoming, outgoing) * outgoing.y;

		brdf_val_cos = diffuse_brdf_cos + ggx_reflect_brdf_cos;
		brdf_pdf = diffuse_pdf*diffuse_cprob + ggx_reflect_pdf*ggx_reflect_cprob;
	}
	else
	{
		// scale sample
		samples.x = (samples.x - diffuse_cprob) / ggx_reflect_cprob;

		// sample using ggx reflection
		float ggx_pdf;
		vec3 ggx_brdf_reflect_cos_contrib = Ggx_reflect_cos_sample(outgoing, ggx_pdf, material, incoming, samples);
		vec3 ggx_brdf_reflect_cos = ggx_brdf_reflect_cos_contrib * ggx_pdf;

		// evaluate diffuse
		float diffuse_pdf;
		vec3 diffuse_brdf_cos = Diffuse_eval(diffuse_pdf, material, incoming, outgoing) * outgoing.y;

		brdf_val_cos = diffuse_brdf_cos + ggx_brdf_reflect_cos;
		brdf_pdf = diffuse_pdf*diffuse_cprob + ggx_pdf*ggx_reflect_cprob;
	}

	pdf = brdf_pdf;
	return brdf_val_cos / brdf_pdf;
}

vec3
Material_eval(out float pdf,
			  const Material material,
			  const vec3 incoming,
			  const vec3 outgoing)
{
	// compute probability of choosing a material
	const float diffuse_weight = luminance(material.m_diffuse_refl) + SMALL_VALUE;
	const float ggx_reflect_weight = luminance(material.m_spec_refl) + SMALL_VALUE;
	const float diffuse_cprob = diffuse_weight / (diffuse_weight + ggx_reflect_weight);
	const float ggx_reflect_cprob = 1.0f - diffuse_cprob;

	// evaluate ggx reflection
	float ggx_reflect_pdf;
	vec3 ggx_reflect_brdf = Ggx_reflect_eval(ggx_reflect_pdf, material, incoming, outgoing);

	// evaluate diffuse reflection
	float diffuse_pdf;
	vec3 diffuse_brdf = Diffuse_eval(diffuse_pdf, material, incoming, outgoing);

	// compute pdf
	pdf = ggx_reflect_pdf*ggx_reflect_cprob + diffuse_pdf*diffuse_cprob;

	// compute brdf
	return ggx_reflect_brdf + diffuse_brdf;
}
