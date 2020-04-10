struct CommonRayPayload
{
	float		m_t;
	float		m_triangle_area;
	int			m_instance_id;
	int			m_primitive_id;
	vec2		m_barycoord;
};

struct CommonShadowRayPayload
{
	float		m_visibility;
};
