#ifndef ONB_GLSL
#define ONB_GLSL
// Orthonormal Basis
struct Onb
{
	vec3 m_x;
	vec3 m_y;
	vec3 m_z;
};

Onb
Onb_create(const vec3 normal)
{
#if 1
	// Building an Orthonormal Basis, Revisited - Pixar Graphics 2017

	// note: sign in glsl is not the same as copysign since it can also return 0
	const float s0 = sign(normal.y) + 0.1f; // possible values -0.9, 0.1, 1.1
	const float s = sign(s0); // possible values -1, 1
	const float a = -1.0f / (s + normal.y);
	const float b = normal.z * normal.x * a;

	Onb result;
	result.m_y = normal;
	result.m_x = vec3(s + normal.x * normal.x * a, -normal.x, b);
	result.m_z = vec3(s * b, -s * normal.z, 1.0f + s * normal.z * normal.z * a);
#else
	Onb result;
	if (normal.y >= 0.99)
	{
		result.m_y = normal;
		result.m_x = normalize(cross(normal, vec3(1, 0, 0)));
		result.m_z = cross(result.m_x, result.m_y);
	}
	else
	{
		result.m_y = normal;
		result.m_x = normalize(cross(normal, vec3(0, 1, 0)));
		result.m_z = cross(result.m_x, result.m_y);
	}
#endif


	return result;
}

vec3
Onb_to_local(const Onb onb, const vec3 dir)
{
	return vec3(dot(onb.m_x, dir), dot(onb.m_y, dir), dot(onb.m_z, dir));
}

vec3
Onb_to_world(const Onb onb, const vec3 dir)
{
	return onb.m_x * dir.x + onb.m_y * dir.y + onb.m_z * dir.z;
}
#endif