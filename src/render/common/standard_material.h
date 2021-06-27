#ifndef STANDARD_MATERIAL_H
#define STANDARD_MATERIAL_H

#include "shared.h"
#include "constant.h"
#include "mapping.h"

struct StandardMaterialInfo
{
    float3 m_diff_refl;
    float3 m_spec_refl;
    float  m_spec_roughness;

    void
    init(const float3 diff_refl, const float3 spec_refl, const float spec_roughness)
    {
        m_diff_refl = diff_refl;
        m_spec_refl = spec_refl;
        m_spec_roughness = spec_roughness;
    }

    float3
    eval(const float3 outgoing, const float3 incident)
    {
        return m_diff_refl * M_1_PI;
    }

    float3
    sample(INOUT(float3) outgoing, const float3 incident, const float2 pss_samples)
    {
        outgoing = cosine_hemisphere_from_square(pss_samples);
        return m_diff_refl;
    }
};

#endif