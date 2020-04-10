#ifndef MATERIAL_GLSL_H
#define MATERIAL_GLSL_H

#define MATERIAL_FLAG_IS_OPAQUE 8

struct Material
{
    vec3 m_diffuse_refl;
    float m_roughness;
    //
    vec3 m_spec_refl;
    float m_ior;
    //
    vec3 m_emission;
    int m_flags;
    //
    vec3 m_spec_trans;
    float m_padding;
    //
};

Material
Material_create()
{
    Material result;

    result.m_diffuse_refl = vec3(0.0f);
    result.m_roughness = 0.0f;
    //
    result.m_spec_refl = vec3(0.0f);
    result.m_ior = 1.0f; // note ior, by default is air, = 1.0f
    //
    result.m_emission = vec3(0.0f);
    result.m_flags = MATERIAL_FLAG_IS_OPAQUE;
    //
    result.m_spec_trans = vec3(0.0f);
    result.m_padding = 0.0f;

    return result;
}

#endif
