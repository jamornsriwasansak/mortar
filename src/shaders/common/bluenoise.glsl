#extension GL_EXT_shader_8bit_storage : require

float blue_rand(int sampleIndex, int sampleDimension);

uint GLOBAL_BLUE_SAMPLE_DIMENSION = 0;
uint GLOBAL_BLUE_SAMPLE_INDEX = 0;
uvec2 GLOBAL_BLUE_PIXEL_INDEX = uvec2(0, 0);

void
srand(uvec2 pixel_index,
	  uvec2 image_size,
	  uint sample_index)
{
	GLOBAL_BLUE_PIXEL_INDEX = pixel_index;
	GLOBAL_BLUE_SAMPLE_INDEX = sample_index;
	GLOBAL_BLUE_SAMPLE_DIMENSION = 0;
}

float
rand()
{
	float value = blue_rand(int(GLOBAL_BLUE_SAMPLE_INDEX), int(GLOBAL_BLUE_SAMPLE_DIMENSION));
	GLOBAL_BLUE_SAMPLE_DIMENSION += 1;
	return value;
}

vec2
rand2()
{
	return vec2(rand(), rand());
}

vec3
rand3()
{
	return vec3(rand(), rand(), rand());
}

#define DECLARE_BLUENOISE_LAYOUT(SET0, SET1, SET2) \
layout(set = SET0, binding = 0) buffer BlueSobol \
{ \
	uint8_t sobol_256spp_256d[]; \
} blue_sobol;\
layout(set = SET1, binding = 0) buffer BlueSt \
{ \
	uint8_t scrambling_tile[]; \
} blue_st; \
layout(set = SET2, binding = 0) buffer BlueRt \
{ \
	uint8_t ranking_tile[]; \
} blue_rt; \
float blue_rand(int sampleIndex, int sampleDimension) \
{ \
	int pixel_i = int(GLOBAL_BLUE_PIXEL_INDEX.x) & 127; \
	int pixel_j = int(GLOBAL_BLUE_PIXEL_INDEX.y) & 127; \
	sampleIndex = sampleIndex & 255; \
	sampleDimension = sampleDimension & 255; \
	int rankedSampleIndex = sampleIndex ^ int(blue_rt.ranking_tile[sampleDimension + (pixel_i + pixel_j * 128) * 8]); \
	int value = int(blue_sobol.sobol_256spp_256d[sampleDimension + rankedSampleIndex * 256]); \
	value = value ^ int(blue_st.scrambling_tile[(sampleDimension % 8) + (pixel_i + pixel_j * 128) * 8]); \
	float v = (0.5f + value) / 256.0f; \
	return v; \
} 
