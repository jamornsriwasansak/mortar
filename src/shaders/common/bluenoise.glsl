#extension GL_EXT_shader_8bit_storage : require

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
	float rnd(int sampleIndex, int sampleDimension) \
	{ \
		int pixel_i = int(gl_LaunchIDNV.x) & 127; \
		int pixel_j = int(gl_LaunchIDNV.y) & 127; \
		sampleIndex = sampleIndex & 255; \
		sampleDimension = sampleDimension & 255; \
		int rankedSampleIndex = sampleIndex ^ int(blue_rt.ranking_tile[sampleDimension + (pixel_i + pixel_j * 128) * 8]); \
		int value = int(blue_sobol.sobol_256spp_256d[sampleDimension + rankedSampleIndex * 256]); \
		value = value ^ int(blue_st.scrambling_tile[(sampleDimension % 8) + (pixel_i + pixel_j * 128) * 8]); \
		float v = (0.5f + value) / 256.0f; \
		return v; \
	}
