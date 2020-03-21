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
