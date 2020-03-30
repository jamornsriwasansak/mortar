#include "shared.glsl.h"

#define USE_PCG
#include "common/pcg.glsl"

struct FastPathTracerRayPayload
{
	vec3		m_color;
	float		m_t;
	vec3		m_sampled_direction;
	int			m_i_bounce; // 0 = shot from camera, 1 = shot from the first visible surface
	vec3		m_importance;
	float		m_sampled_direction_pdf_w;
	ivec2		m_pixel;
	RngState	m_rng_state;
};

struct FastPathTracerShadowRayPayload
{
	float		m_visibility;
};
