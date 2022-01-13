#ifndef STANDARD_MATERIAL_REF_H
#define STANDARD_MATERIAL_REF_H

#include "../cpp_compatible.h"

struct StandardMaterial
{
    // tex_id can be either texture id or value
    // this depends on the sign of tex_id
    uint32_t m_diffuse_tex_id;
    uint32_t m_specular_tex_id;
    uint32_t m_roughness_tex_id;
    uint32_t m_padding;

    float3
    decode_rgb(const uint32_t v)
    {
        const float rcp255 = 0.0039215686274509803921568627451f;
        const int vr       = (v >> 0) & 0xff;
        const int vg       = (v >> 8) & 0xff;
        const int vb       = (v >> 16) & 0xff;
        return float3(vr, vg, vb) * rcp255;
    }

    uint32_t
    encode_rgb(const float3 v)
    {
        const uint3 u3        = uint3(round(saturate(v) * 255.0f));
        const uint32_t flag   = (1 << 24);
        const uint32_t result = (u3.r << 0) | (u3.g << 8) | (u3.b << 16) | flag;
        return result;
    }

    float
    decode_r(const uint32_t v)
    {
        const float rcp65535 = 1.0f / 65535.0f;
        const int vr         = v & 0xffff;
        return float(vr) * rcp65535;
    }

    uint32_t
    encode_r(const float v)
    {
        const uint32_t u      = uint32_t(round(clamp(v, 0.0f, 1.0f) * 65535.0f));
        const uint32_t flag   = (1 << 24);
        const uint32_t result = (u << 0) | flag;
        return result;
    }

    bool
    has_diffuse_texture()
    {
        return (m_diffuse_tex_id & (1 << 24)) == 0;
    }

    bool
    has_specular_texture()
    {
        return (m_specular_tex_id & (1 << 24)) == 0;
    }

    bool
    has_roughness_texture()
    {
        return (m_roughness_tex_id & (1 << 24)) == 0;
    }
};
#endif