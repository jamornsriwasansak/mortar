#extension GL_NV_ray_tracing : require

#define RAY_PRD_LOCATION 0
#define SHADOW_RAY_PRD_LOCATION 1

#define DEFAULT_TMIN 0.001f
#define DEFAULT_TMAX 10000000.0f

struct Ray
{
    vec3 m_origin;
    float m_tmin;
    vec3 m_direction;
    float m_tmax;
};

Ray
Ray_create(const vec3 origin,
           const vec3 direction)
{
    Ray result;
    result.m_origin = origin;
    result.m_direction = direction;
    result.m_tmin = DEFAULT_TMIN;
    result.m_tmax = DEFAULT_TMAX;
    return result;
}

void
trace_ray(const in Ray ray,
          const accelerationStructureNV tlas,
          const int miss_index)
{
    uint ray_flags = gl_RayFlagsOpaqueNV;
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
    uint ray_flags = gl_RayFlagsTerminateOnFirstHitNV | gl_RayFlagsSkipClosestHitShaderNV;
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
