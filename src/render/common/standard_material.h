#ifndef STANDARD_MATERIAL_H
#define STANDARD_MATERIAL_H

#include "constant.h"
#include "mapping.h"
#include "shared.h"

float
sqr(const float x)
{
    return x * x;
}

// ggx lambda function
float
Microfacet_lambda(const float3 w, const float2 alpha)
{
    float2 numerator_comps = alpha * w.xz;
    float  numerator       = dot(numerator_comps, numerator_comps);
    float  denominator     = w.y * w.y;
    float  sqrt_term       = sqrt(1.0f + numerator / denominator);
    return (-1.0f + sqrt_term) * 0.5f;
}

float
Microfacet_g1(const float3 w, const float2 alpha)
{
    return 1.0f / (1.0f + Microfacet_lambda(w, alpha));
}

// G term - separable masking shadowing function
float
Microfacet_g2(const float3 v, const float3 l, const float2 alpha)
{
    // Separable Masking and Shadowing
    // Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs - equation 98
    return Microfacet_g1(v, alpha) * Microfacet_g1(l, alpha);
}

// D term - GGX distribution
float
Microfacet_d(const float3 h, const float2 alpha)
{
    const float term        = sqr(h.x / alpha.x) + sqr(h.z / alpha.y) + sqr(h.y);
    const float denominator = alpha.x * alpha.y * sqr(term);
    return M_1_PI / denominator;
}

// F term - shlick fresnel (for ggx_reflect)
float3
Microfacet_f_shlick(const float3 f0, const float3 incoming)
{
    const float one_minus_cos = 1.0f - abs(incoming.y);
    const float exponential   = sqr(sqr(one_minus_cos)) * one_minus_cos;
    return lerp(float3(exponential, exponential, exponential), float3(1.0f, 1.0f, 1.0f), f0);
}

// Code provided by heitz et al.
// http://jcgt.org/published/0007/04/01/
void
Microfacet_sampleGGXVNDF(INOUT(float3) h, const float3 incoming, const float2 alpha, const float U1, const float U2)
{
    // Input Ve: view direction
    // Input alpha_x, alpha_y: roughness parameters
    // Input U1, U2: uniform random numbers
    // Output Ne: normal sampled with PDF D_Ve(Ne) = G1(Ve) * max(0, dot(Ve, Ne)) * D(Ne) / Ve.z

    float3 Ve = incoming.xzy;

    // Section 3.2: transforming the view direction to the hemisphere configuration
    float3 Vh = normalize(float3(alpha.x * Ve.x, alpha.y * Ve.y, Ve.z));
    // Section 4.1: orthonormal basis (with special case if cross product is zero)
    float  lensq = dot(Vh.xy, Vh.xy);
    float3 T1    = lensq > 0 ? float3(-Vh.y, Vh.x, 0) * rsqrt(lensq) : float3(1, 0, 0);
    float3 T2    = cross(Vh, T1);
    // Section 4.2: parameterization of the projected area
    float r   = sqrt(U1);
    float phi = 2.0 * M_PI * U2;
    float t1  = r * cos(phi);
    float t2  = r * sin(phi);
    float s   = 0.5 * (1.0 + Vh.z);
    t2        = (1.0 - s) * sqrt(1.0 - t1 * t1) + s * t2;
    // Section 4.3: reprojection onto hemisphere
    float3 Nh = t1 * T1 + t2 * T2 + sqrt(max(0.0, 1.0 - r * r)) * Vh;
    // Section 3.4: transforming the normal back to the ellipsoid configuration
    float3 Ne = normalize(float3(alpha.x * Nh.x, alpha.y * Nh.y, max(0.0f, Nh.z)));
    h         = Ne.xzy;
}

struct StandardMaterialInfo
{
    float3 m_diff_refl;
    float3 m_spec_refl;
    float  m_spec_roughness;

    void
    init(const float3 diff_refl, const float3 spec_refl, const float spec_roughness)
    {
        m_diff_refl      = diff_refl;
        m_spec_refl      = spec_refl;
        m_spec_roughness = spec_roughness;
    }

    float3
    eval(const float3 outgoing, const float3 incident)
    {
        return m_diff_refl * M_1_PI;
    }

    float3
    ggx_reflect_cos_sample(INOUT(float3) outgoing, INOUT(float) pdf, const float3 incoming, const float2 samples)
    {
        const float2 alpha = m_spec_roughness * m_spec_roughness;
        const float  side  = sign(incoming.y);
        float3       h;
        Microfacet_sampleGGXVNDF(h, incoming * side, alpha, samples.x, samples.y);
        outgoing = reflect(-incoming, h) * side;

        // f_r              =           f(i, h) * d(h) * g_2(i, o)       / (4 * dot(i, n) * dot(o, n))
        // f_r * dot(o, n)  =           f(i, h) * d(h) * g_1(i) * g_1(o) / (4 * dot(i, n))
        // pdf              = dot(i, h)         * d(h) * g_1(i)          / (    dot(i, n)) * jacobian
        // jacobian         = 1 / (4 * dot(i, h))
        // pdf              =                   * d(h) * g_1(i)          / (4 * dot(i, n))

        // f_r * dot(o, n) / pdf = f(i, h) * g_1(o)

        const float is_same_side = step(0.0f, incoming.y * outgoing.y);

        const float small_value = 0.001f;

        // compute pdf
        const float d_term    = Microfacet_d(h, alpha);
        const float g1_i_term = Microfacet_g1(incoming, alpha);
        pdf = d_term * g1_i_term / (4.0f * abs(incoming.y) + small_value) * is_same_side;

        // compute brdf contrib
        const float3 f_term = m_spec_refl; // Microfacet_f_shlick(material.m_spec_refl, incoming);
        const float  g1_o_term = Microfacet_g1(outgoing, alpha);
        return f_term * g1_o_term * is_same_side;
    }

    float3
    ggx_reflect_eval(INOUT(float) pdf, const float3 incoming, const float3 outgoing)
    {
        const float2 alpha = m_spec_roughness * m_spec_roughness;
        const float3 h     = normalize(incoming + outgoing);

        const float  d_term    = Microfacet_d(h, alpha);
        const float  g1_i_term = Microfacet_g1(incoming, alpha);
        const float  g1_o_term = Microfacet_g1(outgoing, alpha);
        const float3 f_term = m_spec_refl; // Microfacet_f_shlick(material.m_spec_refl, incoming);

        const float is_same_side = step(0.0f, incoming.y * outgoing.y);

        const float small_value = 0.001f;

        // pdf = d(h) * g_1(i) / 4 * dot(i, n)
        pdf = d_term * g1_i_term / (4.0f * abs(incoming.y) + small_value) * is_same_side;

        // brdf = G * D * F / (4 * dot(i, n) * dot(o, n))
        return g1_o_term * f_term * pdf / (outgoing.y + small_value) * is_same_side;
    }

    float3
    sample(INOUT(float3) outgoing, const float3 incoming, const float2 pss_samples)
    {
        outgoing = cosine_hemisphere_from_square(pss_samples);
        return m_diff_refl;
    }
};

#endif