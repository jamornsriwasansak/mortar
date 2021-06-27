#ifndef STANDARD_MATERIAL_REF_H
#define STANDARD_MATERIAL_REF_H

struct StandardMaterial
{
    int m_diffuse_tex_id;
    int m_specular_tex_id;
    int m_roughness_tex_id;
    int m_padding;
};
#endif