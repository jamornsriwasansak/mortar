#extension GL_NV_ray_tracing : require

#include "common/ray.glsl"

void
trace_ray(const in Ray ray,
          const accelerationStructureNV tlas,
          const int miss_index)
{
    uint ray_flags = gl_RayFlagsNoneNV;
    traceNV(tlas,           // accel struct
            ray_flags,      // flag
            0xff,           // face mask
            0,              // sbt record offset
            0,              // sbt record stride
            miss_index,     // miss index (very important! : depends on the order of the shader you put when creating the pipeline)
            ray.m_origin,
            ray.m_tmin,
            ray.m_direction,
            ray.m_tmax,
            RAY_PRD_LOCATION);   // payload location
}

void
trace_shadow_ray(const in Ray ray,
                 const accelerationStructureNV tlas,
                 const int miss_index)
{
    uint ray_flags = gl_RayFlagsNoneNV | gl_RayFlagsTerminateOnFirstHitNV | gl_RayFlagsSkipClosestHitShaderNV;
    traceNV(tlas,           // accel struct
            ray_flags,      // flag
            0xff,           // face mask
            0,              // sbt record offset
            0,              // sbt record stride
            miss_index,     // miss index
            ray.m_origin,
            ray.m_tmin,
            ray.m_direction,
            ray.m_tmax,
            SHADOW_RAY_PRD_LOCATION);    // payload location
}
