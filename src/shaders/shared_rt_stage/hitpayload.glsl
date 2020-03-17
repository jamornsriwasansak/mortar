#include "shared.glsl.h"

struct HitPayload
{
    vec3 m_snormal; // shading normal
    float m_t; // ray t
    Material m_material; // material
    vec3 m_gnormal; // geometry normal
};
