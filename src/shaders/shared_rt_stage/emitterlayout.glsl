#extension GL_EXT_nonuniform_qualifier : require

#include "common/emittersample.glsl"
#include "common/mapping.glsl"
#include "common/ray.glsl"
#include "shared.glsl.h"

#define EMITTER_TYPE_ENVMAP 0
#define EMITTER_TYPE_SURFACE 1

#define DECLARE_EMITTER_LAYOUT(SET, ENVMAP_NAME) \
layout(set = SET, binding = 0) uniform EmitterInfoBlock\
{\
	EmitterInfo info;\
} emitter_info_block;\
layout(set = SET, binding = 1) buffer TBottomLevelCdfTableSize\
{\
	int size[];\
} emitter_blcdf_size; \
layout(set = SET, binding = 2) buffer TTopLevelCdfTable\
{\
	float cdf[];\
} emitter_tlcdf;\
layout(set = SET, binding = 3) buffer TBottomLevelCdfTable\
{\
	float cdf[];\
} emitter_blcdf[];\
int sample_emitter_instance_index(out float prob, out float rescaled_s, const float s)\
{\
	/* upperbound implementation - https://en.cppreference.com/w/cpp/algorithm/upper_bound */\
	int first = 0;\
	/* size of cdf table + 1 larger */ \
	int count = emitter_info_block.info.m_num_emitter + 1;\
	float it_val = 0.0f;\
	while (count > 0)\
	{\
		const int step = count / 2;\
		const int it = first + step;\
		it_val = emitter_tlcdf.cdf[it];\
		if (!(s < it_val)) { first = it + 1; count -= step + 1; }\
		else { count = step; }\
	}\
	const float p0 = emitter_tlcdf.cdf[first - 1];\
	const float p1 = emitter_tlcdf.cdf[first];\
	rescaled_s = (s - p0) / (p1 - p0);\
	prob = p1 - p0;\
	return first - 1;\
}\
int sample_emitter_bottom_level(out float prob, out float rescaled_s, const int cdf_table_index, const float s)\
{\
    int first = 0;\
    int count = emitter_blcdf_size.size[cdf_table_index];\
    while (count > 0)\
    {\
        const int step = count / 2;\
        const int it = first + step;\
        const float it_val = emitter_blcdf[cdf_table_index].cdf[it];\
        if (!(s < it_val)) { first = it + 1; count -= step + 1; }\
        else { count = step; }\
    }\
	const float p0 = emitter_blcdf[cdf_table_index].cdf[first - 1];\
	const float p1 = emitter_blcdf[cdf_table_index].cdf[first];\
	rescaled_s = (s - p0) / (p1 - p0);\
	prob = p1 - p0;\
    return first - 1;\
}\
vec3 sample_emitter(out float pdf, out Ray vis, out int emitter_type, out float s_to_a_jacobian, const vec3 position, const vec3 snormal, vec2 samples)\
{\
	float sample_instance_id_prob = 0.0f;\
	const int instance_id = sample_emitter_instance_index(sample_instance_id_prob, samples.x, samples.x);\
	EmitterSample emitter_sample;\
    if (instance_id < emitter_info_block.info.m_envmap_emitter_offset)\
    {\
		float sample_face_id_prob = 0.0f;\
		const int face_id = sample_emitter_bottom_level(sample_face_id_prob, samples.x, instance_id, samples.x);\
        const uint index0 = faces_arrays[instance_id].faces[face_id * 3 + 0];\
        const uint index1 = faces_arrays[instance_id].faces[face_id * 3 + 1];\
        const uint index2 = faces_arrays[instance_id].faces[face_id * 3 + 2];\
		const vec2 baryccoord = triangle_from_square(samples);\
       	/* position and tex u */\
	    const vec4 pu0 = pus_arrays[instance_id].position_and_us[index0];\
	    const vec4 pu1 = pus_arrays[instance_id].position_and_us[index1];\
	    const vec4 pu2 = pus_arrays[instance_id].position_and_us[index2];\
	    const vec4 pu = mix_barycoord(baryccoord, pu0, pu1, pu2);\
	    /* normal and tex v */\
	    const vec4 nv0 = nvs_arrays[instance_id].normal_and_vs[index0];\
	    const vec4 nv1 = nvs_arrays[instance_id].normal_and_vs[index1];\
	    const vec4 nv2 = nvs_arrays[instance_id].normal_and_vs[index2];\
	    const vec4 nv = mix_barycoord(baryccoord, nv0, nv1, nv2);\
	    /* correct normals and make sure normal share the same direction as incoming vector (= -ray.m_direction)*/\
		const vec3 crossed = cross(pu0.xyz - pu1.xyz, pu0.xyz - pu2.xyz);\
		const float triangle_area = length(crossed) * 0.5f;\
		const float sample_face_pdf = 1.0f / triangle_area;\
	    const vec3 gnormal = normalize(crossed);\
	    /* organize the values */\
	    const vec3 emitter_position = pu.xyz;\
	    const vec2 texcoord = vec2(pu.w, nv.w);\
	    /* material */\
	    const uint material_id = material_ids_arrays[instance_id].material_ids[face_id];\
	    Material material = mat.materials[material_id];\
		DECODE_MATERIAL(material, textures, texcoord);\
		const vec3 emission = material.m_emission;\
		const vec3 to_emitter = emitter_position - position;\
		const vec3 norm_to_emitter = normalize(to_emitter);\
		const float length2 = dot(to_emitter, to_emitter);\
		const float geometry_term = abs(dot(gnormal, norm_to_emitter)) / length2;\
		pdf = sample_instance_id_prob * sample_face_id_prob * sample_face_pdf;\
		vis = Ray_create(position, norm_to_emitter, DEFAULT_TMIN, 1.0f - DEFAULT_TMIN);\
		emitter_type = EMITTER_TYPE_SURFACE;\
		s_to_a_jacobian = geometry_term;\
		return geometry_term * emission / pdf;\
    }\
    else /* instance >= emitter_info_block.info.m_envmap_emitter_offset */\
    {\
    	float sample_pixel_prob = 0.0f;\
    	const uint pixel_id = sample_emitter_bottom_level(sample_pixel_prob, samples.x, instance_id, samples.x);\
		const uint pixel_x = pixel_id % emitter_info_block.info.m_envmap_size.x;\
		const uint pixel_y = pixel_id / emitter_info_block.info.m_envmap_size.x;\
		const vec2 resolution = vec2(emitter_info_block.info.m_envmap_size);\
		const vec2 uv = (vec2(pixel_x, pixel_y) + samples) / resolution;\
		vec3 direction = world_from_panorama(uv);\
		if (any(isnan(direction))) direction = vec3(0.0f, 1.0f, 0.0f);\
		const float sine = sqrt(max(1.0f - sqr(direction.y), 0.0f));\
		const vec3 emission = texture(ENVMAP_NAME, uv).xyz;\
		const float length2 = 1.0f;\
		const float geometry_term = 1.0f;\
	    float pdf_sine = sample_instance_id_prob * sample_pixel_prob * resolution.x * resolution.y * M_1_PI * M_1_PI * 0.5f;\
		pdf = pdf_sine / (sine + SMALL_VALUE);\
		vis = Ray_create(position, direction);\
		emitter_type = EMITTER_TYPE_ENVMAP;\
		s_to_a_jacobian = 1.0f;\
		return vec3(1.0f) * geometry_term * emission / (pdf + SMALL_VALUE);\
    }\
}\
float geometry_emitter_pdf(const int instance_id, const int face_id, const float triangle_area)\
{\
	const float tlpdf = emitter_tlcdf.cdf[instance_id + 1] - emitter_tlcdf.cdf[instance_id];\
	const float blpdf = emitter_blcdf[instance_id].cdf[face_id + 1] - emitter_blcdf[instance_id].cdf[face_id];\
	return tlpdf * blpdf / triangle_area;\
}\
float envmap_emitter_pdf(const vec2 uv)\
{\
	const int instance_id = emitter_info_block.info.m_envmap_emitter_offset;\
	const uvec2 uresolution = emitter_info_block.info.m_envmap_size;\
	const vec2 resolution = vec2(emitter_info_block.info.m_envmap_size);\
	const uvec2 pixel = uvec2(uv * resolution);\
	const uint pixel_id = pixel.y * uresolution.x + pixel.x;\
	const float tlpdf = emitter_tlcdf.cdf[instance_id + 1] - emitter_tlcdf.cdf[instance_id];\
	const float blpdf = emitter_blcdf[instance_id].cdf[pixel_id + 1] - emitter_blcdf[instance_id].cdf[pixel_id];\
	const float sine = sin(uv.y * M_PI);\
	return tlpdf * blpdf * resolution.x * resolution.y * M_1_PI * M_1_PI * 0.5f / (sine + SMALL_VALUE);\
}
