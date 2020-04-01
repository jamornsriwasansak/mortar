#ifndef EMITTER_SAMPLE_GLSL
#define EMITTER_SAMPLE_GLSL

#define EMITTER_GEOMETRY 0
#define EMITTER_ENVMAP 1

struct EmitterSample
{
	vec3 m_position;
	float m_pdf;
	vec3 m_gnormal;
	int m_flag;
	vec3 m_snormal;
	vec3 m_emission;
};
#endif