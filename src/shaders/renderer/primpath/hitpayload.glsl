#include "shared.glsl.h"

struct PrimitivePathTracerPayload
{
    Material m_material;
    //
    vec3 m_normal;
    float m_t;
};
