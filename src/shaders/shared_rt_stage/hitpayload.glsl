#include "shared.glsl.h"

struct HitPayload
{
    vec3 m_snormal; // shading normal
    float m_t; // ray t
    Material m_material; // material
    vec3 m_gnormal; // geometry normal
    int m_instance_id;
    int m_face_id;
    float m_face_area;
};
