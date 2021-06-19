#ifndef MAPPING_H
#define MAPPING_H

#include "constant.h"

float2
triangle_from_square(const float2 samples)
{
#if 1
    // A Low - Distortion Map Between Triangle and Square, Eric Heitz
    if (samples.y > samples.x)
    {
        float x = samples.x * 0.5f;
        float y = samples.y - x;
        return float2(x, y);
    }
    else
    {
        float y = samples.y * 0.5f;
        float x = samples.x - y;
        return float2(x, y);
    }
#else
    // a well known sqrt mapping for uniformly sample a triangle
    float sqrt_sample0 = sqrt(samples.x);
    float b1           = sqrt_sample0 * (1.0f - samples.y);
    float b2           = samples.y * sqrt_sample0;
    return float2(b1, b2);
#endif
}

float3
cosine_hemisphere_from_square(const float2 samples)
{
    const float sin_phi   = sqrt(1.0f - samples[0]);
    const float theta     = M_PI * 2.0f * samples[1]; // theta
    const float sin_theta = sin(theta);
    const float cos_theta = cos(theta);
    return float3(cos_theta * sin_phi, sqrt(samples[0]), sin_theta * sin_phi);
}

float3
sphere_from_square(const float2 samples)
{
    const float z      = 1.0f - 2.0f * samples.y;
    const float r      = sqrt(samples.y * (1.0f - samples.y));
    const float phi    = 2.0f * M_PI * samples.x; // theta = [0, 2pi)
    const float cosphi = cos(phi);
    const float sinphi = sin(phi);
    return float3(2.0f * cosphi * r, 2.0f * sinphi * r, z);
}

float2
panorama_from_world(const float3 dir)
{
    const float u = atan2(-dir[2], -dir[0]) * M_1_PI * 0.5f + 0.5f;
    const float v = acos(dir[1]) * M_1_PI;
    return float2(u, v);
}

float3
world_from_spherical(const float2 theta_phi)
{
    const float theta     = theta_phi.x;
    const float phi       = theta_phi.y;
    const float sin_phi   = sin(phi);
    const float cos_phi   = cos(phi);
    const float sin_theta = sin(theta);
    const float cos_theta = cos(theta);
    return float3(cos_theta * sin_phi, cos_phi, sin_theta * sin_phi);
}

float3
world_from_panorama(const float2 uv)
{
    const float theta = uv[0] * M_PI * 2.0f;
    const float phi   = uv[1] * M_PI;
    return world_from_spherical(float2(theta, phi));
}
#endif