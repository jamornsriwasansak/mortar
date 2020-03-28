#extension GL_EXT_nonuniform_qualifier : require

#include "common/emittersample.glsl"
#include "common/mapping.glsl"
#include "shared.glsl.h"

#define DECLARE_TRIMESH_LAYOUT(SET) \
layout(set = SET, binding = 0) buffer TFace\
{\
	uint faces[];\
} faces_arrays[];\
layout(set = SET, binding = 1) buffer TPuArray\
{\
	vec4 position_and_us[];\
} pus_arrays[];\
layout(set = SET, binding = 2) buffer TNvArray\
{\
	vec4 normal_and_vs[];\
} nvs_arrays[];\
layout(set = SET, binding = 3) buffer TMaterialId\
{\
	uint material_ids[];\
} material_ids_arrays[];\
layout(set = SET, binding = 4) buffer TMaterialBuffer\
{\
	Material materials[];\
} mat;\
layout(set = SET, binding = 5) uniform sampler2D textures[];

#define DECLARE_EMITTER_LAYOUT(SET) \
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
int sample_emitter_face(out float prob, out float rescaled_s, const int cdf_table_index, const float s)\
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
EmitterSample sample_emitter(vec2 samples)\
{\
	float sample_instance_id_prob = 0.0f;\
	const int instance_id = sample_emitter_instance_index(sample_instance_id_prob, samples.x, samples.x);\
	EmitterSample emitter_sample;\
    if (instance_id < emitter_info_block.info.m_envmap_emitter_offset)\
    {\
		float sample_face_id_prob = 0.0f;\
		const int face_id = sample_emitter_face(sample_face_id_prob, samples.x, instance_id, samples.x);\
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
	    emitter_sample.m_gnormal = normalize(crossed);\
	    emitter_sample.m_snormal = normalize(nv.xyz);\
	    emitter_sample.m_snormal = dot(emitter_sample.m_gnormal, emitter_sample.m_snormal) >= 0.0f ? emitter_sample.m_snormal : -emitter_sample.m_snormal;\
	    /* organize the values */\
	    emitter_sample.m_position = pu.xyz;\
	    vec2 texcoord = vec2(pu.w, nv.w);\
	    /* material */\
	    const uint material_id = material_ids_arrays[instance_id].material_ids[face_id];\
	    Material material = mat.materials[material_id];\
		DECODE_MATERIAL(material, textures, texcoord);\
		emitter_sample.m_emission = material.m_emission;\
        emitter_sample.m_flag = EMITTER_GEOMETRY;\
		emitter_sample.m_pdf = sample_instance_id_prob * sample_face_id_prob * sample_face_pdf;\
		return emitter_sample;\
    }\
    emitter_sample.m_position = vec3(0.0f);\
    emitter_sample.m_flag = EMITTER_ENVMAP;\
	return emitter_sample;\
}\
float geometry_emitter_pdf(const int instance_id, const int face_id, const float triangle_area)\
{\
	const float tlpdf = emitter_tlcdf.cdf[instance_id + 1] - emitter_tlcdf.cdf[instance_id];\
	const float blpdf = emitter_blcdf[instance_id].cdf[face_id + 1] - emitter_blcdf[instance_id].cdf[face_id];\
	return tlpdf * blpdf / triangle_area;\
}
