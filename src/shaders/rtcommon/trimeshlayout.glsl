#extension GL_EXT_nonuniform_qualifier : require

#include "material.glsl.h"
#include "common/math.glsl"
#include "common/emittersample.glsl"
#include "common/mapping.glsl"

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
layout(set = SET, binding = 5) uniform sampler2D material_textures[];\
void extract_hit_info(out vec3 position,\
					  out vec3 snormal,\
					  out vec3 gnormal,\
					  out vec2 texcoord,\
                      out float triangle_area,\
					  const int instance_id,\
					  const int primitive_id,\
					  const vec2 barycoord)\
{\
    /* fetch from ssbo */\
    const uint index0 = faces_arrays[instance_id].faces[primitive_id * 3 + 0];\
    const uint index1 = faces_arrays[instance_id].faces[primitive_id * 3 + 1];\
    const uint index2 = faces_arrays[instance_id].faces[primitive_id * 3 + 2];\
    /* position and tex u */\
    const vec4 pu0 = pus_arrays[instance_id].position_and_us[index0];\
    const vec4 pu1 = pus_arrays[instance_id].position_and_us[index1];\
    const vec4 pu2 = pus_arrays[instance_id].position_and_us[index2];\
    const vec4 pu = mix_barycoord(barycoord, pu0, pu1, pu2);\
    position = pu.xyz;\
    /* snormal and tex v */\
    const vec4 nv0 = nvs_arrays[instance_id].normal_and_vs[index0];\
    const vec4 nv1 = nvs_arrays[instance_id].normal_and_vs[index1];\
    const vec4 nv2 = nvs_arrays[instance_id].normal_and_vs[index2];\
    const vec4 nv = mix_barycoord(barycoord, nv0, nv1, nv2);\
	snormal = normalize(nv.xyz);\
    texcoord = vec2(pu.w, nv.w);\
    /* gnormal and triangle area */\
    const vec3 crossed = cross(pu0.xyz - pu1.xyz, pu0.xyz - pu2.xyz);\
    const float glen = length(crossed);\
    gnormal = crossed / glen;\
    snormal = dot(gnormal, snormal) > 0.0f ? snormal : -snormal;\
    triangle_area = glen * 0.5f;\
}\
void decode_material(inout Material material,\
                     const vec2 uv)\
{\
	int diffuse_refl_tex_index = -int(material.m_diffuse_refl.x + 1);\
	material.m_diffuse_refl = diffuse_refl_tex_index >= 0 ? texture(material_textures[diffuse_refl_tex_index], uv).xyz : material.m_diffuse_refl;\
    int roughness_tex_index = -int(material.m_roughness + 1);\
	material.m_roughness = roughness_tex_index >= 0 ? texture(material_textures[roughness_tex_index], uv).x : material.m_roughness;\
	int spec_refl_tex_index = -int(material.m_spec_refl.x + 1);\
	material.m_spec_refl = spec_refl_tex_index >= 0 ? texture(material_textures[spec_refl_tex_index], uv).xyz : material.m_spec_refl;\
    int ior_tex_index = -int(material.m_ior + 1);\
	material.m_ior = ior_tex_index >= 0 ? texture(material_textures[ior_tex_index], uv).x : material.m_ior;\
	int emission_tex_index = -int(material.m_emission.x + 1);\
	material.m_emission = emission_tex_index >= 0 ? texture(material_textures[emission_tex_index], uv).xyz : material.m_emission;\
    int spec_trans_index = -int(material.m_spec_trans.x + 1);\
	material.m_spec_trans = spec_trans_index >= 0 ? texture(material_textures[spec_trans_index], uv).xyz : material.m_spec_trans;\
}
