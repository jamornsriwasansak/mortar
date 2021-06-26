#ifndef STANDARD_MATERIAL
#define STANDARD_MATERIAL

struct StandardMaterial
{
    int m_diffuse_tex_id;
    int m_specular_tex_id;
    int m_roughness_tex_id;
    int m_padding;
};
#endif