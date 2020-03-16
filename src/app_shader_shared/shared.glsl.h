#ifndef SHARED_GLSL_H
#define SHARED_GLSL_H
// everything is this file is shared between shaders and c++ source
// we have to make sure that everything is align by 16 (sizeof struct must be multiple of 16)
// this is due to vulkan's ssbo standard

// we follow disney materials for now.

#define MATERIAL_FLAG_IS_OPAQUE 8

struct Material
{
    vec3 m_diffuse_refl;
    float m_roughness;
    //
    vec3 m_spec_refl;
    float m_sheen;
    //
    vec3 m_emission;
    int m_flags;
};

Material
Material_create()
{
    Material result;
    result.m_diffuse_refl = vec3(0.0f);
    result.m_roughness = 0.0f;
    result.m_spec_refl = vec3(0.0f);
    result.m_sheen = 0.0f;
    result.m_emission = vec3(0.0f);
    result.m_flags = MATERIAL_FLAG_IS_OPAQUE;
    return result;
}

#define DECODE_MATERIAL(MAT, TEXTURES, UV) \
{ \
	int diffuse_refl_tex_index = -int(MAT.m_diffuse_refl.x + 1); \
	MAT.m_diffuse_refl = diffuse_refl_tex_index >= 0 ? texture(TEXTURES[diffuse_refl_tex_index], UV).xyz : MAT.m_diffuse_refl; \
    int roughness_tex_index = -int(MAT.m_roughness + 1); \
	MAT.m_roughness = roughness_tex_index >= 0 ? texture(TEXTURES[roughness_tex_index], UV).x : MAT.m_roughness; \
	int spec_refl_tex_index = -int(MAT.m_spec_refl.x + 1); \
	MAT.m_spec_refl = spec_refl_tex_index >= 0 ? texture(TEXTURES[spec_refl_tex_index], UV).xyz : MAT.m_spec_refl; \
    int sheen_tex_index = -int(MAT.m_sheen + 1); \
	MAT.m_sheen = sheen_tex_index >= 0 ? texture(TEXTURES[sheen_tex_index], UV).x : MAT.m_sheen; \
	int emission_tex_index = -int(MAT.m_emission.x + 1); \
	MAT.m_emission = emission_tex_index >= 0 ? texture(TEXTURES[emission_tex_index], UV).xyz : MAT.m_emission; \
}

#endif
