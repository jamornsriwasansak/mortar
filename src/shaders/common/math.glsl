vec2 mix_barycoord(const vec2 bary, const vec2 a, const vec2 b, const vec2 c)
{
    return (1.0f - bary.x - bary.y) * a + bary.x * b + bary.y * c;
}

vec3 mix_barycoord(const vec2 bary, const vec3 a, const vec3 b, const vec3 c)
{
    return (1.0f - bary.x - bary.y) * a + bary.x * b + bary.y * c;
}

vec4 mix_barycoord(const vec2 bary, const vec4 a, const vec4 b, const vec4 c)
{
    return (1.0f - bary.x - bary.y) * a + bary.x * b + bary.y * c;
}

float distance2(const vec3 a, const vec3 b)
{
	vec3 diff = a - b;
	return dot(diff, diff);
}