#ifndef STANDARD_EMISSION_REF_H
#define STANDARD_EMISSION_REF_H

#include "../cpp_compatible.h"

struct StandardEmission
{
    uint32_t m_emission_tex_id;

    float3
    decode_rgb(const uint32_t v) CONST_FUNC
    {
        //assert(false && "this is for encoding ldr");
        const float rcp255 = 0.0039215686274509803921568627451f;
        const int   vr     = (v >> 0) & 0xff;
        const int   vg     = (v >> 8) & 0xff;
        const int   vb     = (v >> 16) & 0xff;
        return float3(vr, vg, vb) * rcp255;
    }

    uint32_t
    encode_rgb(const float3 v) CONST_FUNC
    {
        //assert(false && "this is for encoding ldr");
        const uint3    u3     = uint3(round(saturate(v) * 255.0f));
        const uint32_t flag   = (1 << 24);
        const uint32_t result = (u3.r << 0) | (u3.g << 8) | (u3.b << 16) | flag;
        return result;
    }

    bool
    is_emission_texture() CONST_FUNC
    {
        return (m_emission_tex_id & (1 << 24)) == 0;
    }
};

#endif