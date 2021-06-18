#ifndef PBR_MATERIAL
#define PBR_MATERIAL

struct PbrMaterial
{
    int m_diffuse_tex_id;
    int m_roughness_tex_id;
    int m_padding_0;
    int m_padding_1;
};
#endif