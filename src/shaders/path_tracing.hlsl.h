#include "common/mapping.h"
#include "common/onb.h"
#include "cpp_compatible.h"
#include "path_tracing_params.h"
#include "rng/pcg.h"

RAY_GEN_SHADER
void
RayGen()
{
    const uint2 pixel_pos   = DispatchRaysIndex().xy;
    const uint2 resolution  = DispatchRaysDimensions().xy;
    const uint  pixel_index = pixel_pos.y * resolution.x + pixel_pos.x;

    const float2 center_uv        = (float2(pixel_pos) + 0.5f.xx) / float2(resolution);
    const float2 center_ndc_snorm = center_uv * 2.0f - 1.0f;

    const float depth = u_gbuffer_depth[pixel_pos];
    if (depth == 0.0f)
    {
        u_diffuse_direct_light_result[pixel_pos] = 0.0f.xxx;
        return;
    }

    // Camera parameters
    const float3 origin = mul(u_params.m_camera_inv_view, float4(0.0f, 0.0f, 0.0f, 1.0f)).xyz;
    const float3 lookat = mul(u_params.m_camera_inv_proj, float4(center_ndc_snorm, 1.0f, 1.0f)).xyz;

    // Recover hit position, hit normal, and hit direction
    float3 dir_to_hit_pos = normalize(mul(u_params.m_camera_inv_view, float4(lookat, 0.0f)).xyz);
    float3 hit_pos        = dir_to_hit_pos * depth + origin;

    // Shading Normal
    float3 shading_normal = u_gbuffer_shading_normal[pixel_pos];
    shading_normal        = faceforward(shading_normal, dir_to_hit_pos, shading_normal);

    // Construct Orthonormal basis
    Onb snormal_onb = Onb_create(shading_normal);

    // Random number generator
    PcgRng rng;
    rng.init(0, pixel_index);

    float4 result = 0.f.xxxx;
    for (int i = 0; i < 10; i++)
    {
        // sampling next direction
        const float3 local_sampled_dir = cosine_hemisphere_from_square(rng.next_float2());
        const float3 sampled_dir       = snormal_onb.to_global(local_sampled_dir);

        // Light Importance Sampling
        {}

        // BSDF Importance Sampling & Find indirect hit pos
        {
        }

        // Ray Desc
        RayDesc ray;
        ray.Origin    = hit_pos;
        ray.Direction = sampled_dir;
        ray.TMin      = 0.001f;
        ray.TMax      = 10000.0f;

        PathTracingPayload payload;
        payload.m_hit = 0.0f.xxxx;
        TraceRay(u_scene_bvh, RAY_FLAG_FORCE_OPAQUE, 0xFF, 0, 0, 0, ray, payload);
        result += payload.m_hit;
    }

    u_diffuse_direct_light_result[pixel_pos] = result.xyz + (hit_pos + shading_normal + depth) * 0.0000000001f;
}

CLOSEST_HIT_SHADER
void
ClosestHit(INOUT(PathTracingPayload) payload, const PathTracingAttributes attributes)
{
}

MISS_SHADER
void Miss(INOUT(PathTracingPayload) payload) { payload.m_hit = 1.0f.xxxx; }