#ifndef ONB_H
#define ONB_H
// Orthonormal Basis
struct Onb
{
    float3 m_x;
    float3 m_y;
    float3 m_z;

    float3
    to_local(const float3 dir)
    {
        return float3(dot(m_x, dir), dot(m_y, dir), dot(m_z, dir));
    }

    float3
    to_global(const float3 dir)
    {
        return m_x * dir.x + m_y * dir.y + m_z * dir.z;
    }
};

Onb
Onb_create(const float3 normal)
{
#if 1
    // Building an Orthonormal Basis, Revisited - Pixar Graphics 2017
    const float s = normal.y > 0.0f ? 1.0f : -1.0f; // possible values -1, 1
    const float a = -1.0f / (s + normal.y);
    const float b = normal.z * normal.x * a;
    Onb         result;
    result.m_y = normal;
    result.m_x = float3(s + normal.x * normal.x * a, -normal.x, b);
    result.m_z = float3(s * b, -s * normal.z, 1.0f + s * normal.z * normal.z * a);
#else
    Onb result;
    if (normal.y >= 0.99)
    {
        result.m_y = normal;
        result.m_x = normalize(cross(normal, float3(1, 0, 0)));
        result.m_z = cross(result.m_x, result.m_y);
    }
    else
    {
        result.m_y = normal;
        result.m_x = normalize(cross(normal, float3(0, 1, 0)));
        result.m_z = cross(result.m_x, result.m_y);
    }
#endif
    return result;
}
#endif
