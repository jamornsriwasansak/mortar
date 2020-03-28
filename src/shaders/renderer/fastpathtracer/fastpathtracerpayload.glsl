#include "shared.glsl.h"

#define FPT_RAY_PRD_LOCATION
#define FPT_SHAROW_PRD_LOCATION

struct FastPathTracerRayPayload
{
	vec3		m_color;
	float		m_t;
	vec3		m_sampled_direction;
	int			m_i_bounce; // 0 = shot from camera, 1 = shot from the first visible surface
	vec3		m_importance;
	float		m_sampled_direction_pdf_w;
	ivec2		m_pixel;
};

struct FastPathTracerShadowRayPayload
{
	float		m_visibility;
};
