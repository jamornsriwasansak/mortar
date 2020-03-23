#define EMITTER_GEOMETRY 0
#define EMITTER_ENVMAP 1

struct EmitterSample
{
	vec3 m_position;
	vec3 m_gnormal;
	vec3 m_snormal;
	vec3 m_emission;
	int m_flag;
};