#extension GL_EXT_shader_8bit_storage : require

float blue_rand(uvec2 pixel_index, int sampleIndex, int sampleDimension);
vec2 blue_rand2(uvec2 pixel_index, int sampleIndex, ivec2 sampleDimension);

#ifdef USE_BLUE_RAND
struct RngState
{
	uvec2 m_pixel_index;
	int m_sample_index;
	int m_dimension_count;
};

RngState
srand(uvec2 pixel_index,
	  uvec2 image_size,
	  uint sample_index)
{
	RngState result;
	result.m_pixel_index = pixel_index;
	result.m_sample_index = int(sample_index);
	result.m_dimension_count = 0;
	return result;
}

float
rand(inout RngState rng)
{
	float value = blue_rand(rng.m_pixel_index, rng.m_sample_index, rng.m_dimension_count);
	rng.m_dimension_count += 1;
	return value;
}

vec2
rand2(inout RngState rng)
{
	vec2 value = blue_rand2(rng.m_pixel_index, rng.m_sample_index, ivec2(rng.m_dimension_count, rng.m_dimension_count + 1));
	rng.m_dimension_count += 2;
	return value;
}
#endif

#define DECLARE_BLUENOISE_LAYOUT(SET) \
layout(set = SET, binding = 0) buffer BlueSobol \
{ \
	uint8_t sobol_256spp_256d[]; \
} blue_sobol;\
layout(set = SET, binding = 1) buffer BlueSt \
{ \
	uint8_t scrambling_tile[]; \
} blue_st; \
layout(set = SET, binding = 2) buffer BlueRt \
{ \
	uint8_t ranking_tile[]; \
} blue_rt; \
float blue_rand(uvec2 pixel_index, int sampleIndex, int sampleDimension) \
{\
	int pixel_i = int(pixel_index.x) & 127; \
	int pixel_j = int(pixel_index.y) & 127; \
	sampleIndex = sampleIndex & 255; \
	sampleDimension = sampleDimension & 255; \
	int rankedSampleIndex = sampleIndex ^ int(blue_rt.ranking_tile[sampleDimension + (pixel_i + pixel_j * 128) * 8]); \
	int value = int(blue_sobol.sobol_256spp_256d[sampleDimension + rankedSampleIndex * 256]); \
	value = value ^ int(blue_st.scrambling_tile[(sampleDimension % 8) + (pixel_i + pixel_j * 128) * 8]); \
	float v = (0.5f + value) / 256.0f; \
	return v; \
} \
vec2 blue_rand2(uvec2 pixel_index, int sampleIndex, ivec2 sampleDimension) \
{\
	int pixel_i = int(pixel_index.x) & 127; \
	int pixel_j = int(pixel_index.y) & 127; \
	sampleIndex = sampleIndex & 255; \
	sampleDimension = sampleDimension & ivec2(255); \
	int rankedSampleIndex0 = sampleIndex ^ int(blue_rt.ranking_tile[sampleDimension.x + (pixel_i + pixel_j * 128) * 8]); \
	int rankedSampleIndex1 = sampleIndex ^ int(blue_rt.ranking_tile[sampleDimension.y + (pixel_i + pixel_j * 128) * 8]); \
	int value0 = int(blue_sobol.sobol_256spp_256d[sampleDimension.x + rankedSampleIndex0 * 256]); \
	int value1 = int(blue_sobol.sobol_256spp_256d[sampleDimension.y + rankedSampleIndex1 * 256]); \
	ivec2 value = ivec2(value0, value1); \
	int s0 = int(blue_st.scrambling_tile[(sampleDimension.x % 8) + (pixel_i + pixel_j * 128) * 8]); \
	int s1 = int(blue_st.scrambling_tile[(sampleDimension.y % 8) + (pixel_i + pixel_j * 128) * 8]); \
	value = value ^ ivec2(s0, s1); \
	vec2 v = (vec2(0.5f) + value) / 256.0f; \
	return v; \
} 
