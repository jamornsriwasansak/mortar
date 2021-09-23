#ifndef STANDARD_MATERIAL_REF_H
#define STANDARD_MATERIAL_REF_H

struct StandardMaterial
{
    int32_t m_diffuse_tex_id;
    int32_t m_specular_tex_id;
    int32_t m_roughness_tex_id;
    int32_t m_padding;
};
#endif