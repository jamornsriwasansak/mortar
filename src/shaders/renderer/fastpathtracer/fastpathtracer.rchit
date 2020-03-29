#version 460
#extension GL_NV_ray_tracing : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

#include "common/math.glsl"
#include "common/ray.glsl"
#include "common/traceray.glsl"
#include "common/onb.glsl"
#include "renderer/fastpathtracer/unsafebsdf.glsl"
#include "renderer/fastpathtracer/fastpathtracerpayload.glsl"
#include "shared.glsl.h"
#include "shared_rt_stage/trimeshlayout.glsl"

#define USE_BLUE_RAND
#include "common/bluenoise.glsl"

layout(set = 0, binding = 0) uniform accelerationStructureNV tlas;
layout(set = 0, binding = 1, rgba32f) uniform image2D image;
layout(set = 0, binding = 2) uniform CameraProperties
{
    mat4 m_view;
    mat4 m_proj;
    mat4 m_inv_view;
    mat4 m_inv_proj;
    int m_is_moved;
    int m_i_frame;
    float _padding[2];
} cam;
layout(set = 0, binding = 3) uniform sampler2D envmap;


layout(location = RAY_PRD_LOCATION) rayPayloadInNV FastPathTracerRayPayload prd;
layout(location = SHADOW_RAY_PRD_LOCATION) rayPayloadNV FastPathTracerShadowRayPayload shadow_prd;
hitAttributeNV vec3 attribs;

// set 1: triangle attributes
DECLARE_TRIMESH_LAYOUT(1)

// set 2: emitter sampling information
DECLARE_EMITTER_LAYOUT(2)

// set 3: bluenoise sampler
DECLARE_BLUENOISE_LAYOUT(3)

float eval_mi_weight(const float pdf1, const float pdf2)
{
    return pdf1 / (pdf1 + pdf2);
}

const int num_bounces = 2;

void main()
{
    // update t
    prd.m_t = gl_HitTNV;

    // fetch from ssbo
    const uint index0 = faces_arrays[gl_InstanceID].faces[gl_PrimitiveID * 3 + 0];
    const uint index1 = faces_arrays[gl_InstanceID].faces[gl_PrimitiveID * 3 + 1];
    const uint index2 = faces_arrays[gl_InstanceID].faces[gl_PrimitiveID * 3 + 2];

    // position and tex u
    const vec4 pu0 = pus_arrays[gl_InstanceID].position_and_us[index0];
    const vec4 pu1 = pus_arrays[gl_InstanceID].position_and_us[index1];
    const vec4 pu2 = pus_arrays[gl_InstanceID].position_and_us[index2];
    const vec4 pu = mix_barycoord(attribs.xy, pu0, pu1, pu2);

    // normal and tex v
    const vec4 nv0 = nvs_arrays[gl_InstanceID].normal_and_vs[index0];
    const vec4 nv1 = nvs_arrays[gl_InstanceID].normal_and_vs[index1];
    const vec4 nv2 = nvs_arrays[gl_InstanceID].normal_and_vs[index2];
    const vec4 nv = mix_barycoord(attribs.xy, nv0, nv1, nv2);

    // get geometric normal and shading normal
    const vec3 crossed = cross(pu0.xyz - pu1.xyz, pu0.xyz - pu2.xyz);
    const float triangle_area = length(crossed) * 0.5f;
    vec3 gnormal = normalize(crossed);
    vec3 snormal = normalize(nv.xyz);

    // correct normals and make sure normals share the same direction as incoming vector (= -ray.m_direction)
    // equivalent to faceforward function
    gnormal = gnormal * sign(dot(gnormal, -gl_WorldRayDirectionNV));
    snormal = snormal * sign(dot(gnormal, snormal));

    // organize the values
    const vec3 position = pu.xyz;
    const vec2 texcoord = vec2(pu.w, nv.w);

    // decode material
    const uint material_id = material_ids_arrays[gl_InstanceID].material_ids[gl_PrimitiveID];
    Material material = mat.materials[material_id];
    DECODE_MATERIAL(material, textures, texcoord);

    // prepare onb and convert camera to local
    Onb onb = Onb_create(snormal);
	const vec3 local_to_incoming = Onb_to_local(onb, -gl_WorldRayDirectionNV);

    // since every bounce we consume 4 random numbers (2 for sampling light source, 2 for sampling next direction)
    int random_number_offset = prd.m_i_bounce * 4;

    /*
    * Emitter Sampling
    */

    // sample emitter
    const float rn0 = blue_rand(prd.m_pixel, cam.m_i_frame, random_number_offset + 0);
    const float rn1 = blue_rand(prd.m_pixel, cam.m_i_frame, random_number_offset + 1);
    EmitterSample emitter_sample = sample_emitter(vec2(rn0, rn1));
    vec3 light_contrib = vec3(0.0f);

    if (emitter_sample.m_flag == EMITTER_GEOMETRY)
    {
        const vec3 to_emitter = emitter_sample.m_position - position;

        // evaluate shadow ray for geometry term
        Ray shadow_ray = Ray_create(position, to_emitter, DEFAULT_TMIN, 1.0f - DEFAULT_TMIN);
        shadow_prd.m_visibility = 0.0f;
        trace_shadow_ray(shadow_ray, tlas, 1);
        const float visibility = shadow_prd.m_visibility;

        // eval geometric connection between emitter and current position
        // normally, geometry term as defined in light transport uses abs(dot()) but here we uses max(dot()) so that we avoid light leak automatically
        const float sqr_dist = dot(to_emitter, to_emitter);
        const float geometry_term_numerator = max(dot(snormal, to_emitter), 0.0f) * max(-dot(emitter_sample.m_gnormal, to_emitter), 0.0f);

        // evaluate brdf term
        float brdf_pdf_w;
        const vec3 local_to_emitter = Onb_to_local(onb, normalize(to_emitter));
        const vec3 brdf_term = Fast_Material_eval(brdf_pdf_w, material, local_to_incoming, local_to_emitter);

        // compute mis weight if we are at the first bounce
        if (prd.m_i_bounce < num_bounces - 1)
        {
            // change of variable for brdf_pdf
            const float brdf_pdf_a = brdf_pdf_w * local_to_emitter.y / sqr_dist;
            const float light_mi_weight = eval_mi_weight(emitter_sample.m_pdf, brdf_pdf_a);
            const float geometry_term = geometry_term_numerator / (sqr_dist * sqr_dist);
			light_contrib = light_mi_weight * geometry_term * visibility * brdf_term * emitter_sample.m_emission / emitter_sample.m_pdf;
        }
        else // prd.m_i_bounce == num_bounces - 1
        {
            const float geometry_term = geometry_term_numerator / sqr_dist * sqr_dist;
			light_contrib = geometry_term * visibility * brdf_term * emitter_sample.m_emission / emitter_sample.m_pdf;
        }
    }
    else if (emitter_sample.m_flag == EMITTER_ENVMAP)
    {
        // for envmap to_emitter is already normalized
        const vec3 to_emitter = emitter_sample.m_position;

        // evaluate shadow ray for geometry term
        Ray shadow_ray = Ray_create(position, to_emitter);
        shadow_prd.m_visibility = 0.0f;
        trace_shadow_ray(shadow_ray, tlas, 1);
        const float visibility = shadow_prd.m_visibility;

        // eval geometric connection between emitter and current position
        // normally, geometry term as defined in light transport uses abs(dot()) but here we uses max(dot()) so that we avoid light leak automatically
        const float sqr_dist = 1.0f;
        const float geometry_term_numerator = max(dot(normalize(snormal), to_emitter), 0.0f);
        const float geometry_term = geometry_term_numerator / sqr_dist;

        // evaluate brdf term
        float brdf_pdf_w;
        const vec3 local_to_emitter = Onb_to_local(onb, to_emitter);
        const vec3 brdf_term = Fast_Material_eval(brdf_pdf_w, material, local_to_incoming, local_to_emitter);

    	light_contrib = brdf_term * visibility * emitter_sample.m_emission * geometry_term / emitter_sample.m_pdf;
    }

    prd.m_color += prd.m_importance * light_contrib;

    /*
    * BRDF Sampling
    */

    if (prd.m_i_bounce == 0)
    {
        prd.m_color += prd.m_importance * material.m_emission;
    }
    else // if (prd.m_i_bounce > 0)
    {
        // hit the surface
		const vec3 contrib = prd.m_importance * material.m_emission;
		const float light_sampling_pdf_a = geometry_emitter_pdf(gl_InstanceID, gl_PrimitiveID, triangle_area);
        const float brdf_sampling_pdf_a = prd.m_sampled_direction_pdf_w / (gl_HitTNV * gl_HitTNV) * abs(dot(gnormal, gl_WorldRayDirectionNV));
        const float brdf_mi_weight = eval_mi_weight(brdf_sampling_pdf_a, light_sampling_pdf_a);
        prd.m_color += prd.m_importance * brdf_mi_weight * material.m_emission;
    }

    // always sample if we do not reach last bounce yet
    if (prd.m_i_bounce < num_bounces - 1)
    {
		// random number for sampling next direction
		const float rn2 = blue_rand(prd.m_pixel, cam.m_i_frame, random_number_offset + 2);
		const float rn3 = blue_rand(prd.m_pixel, cam.m_i_frame, random_number_offset + 3);

		// sample next direction
		float brdf_cos_sampling_pdf;
		vec3 local_next_direction;
		prd.m_importance *= Fast_Material_cos_sample(local_next_direction, brdf_cos_sampling_pdf, material, local_to_incoming, vec2(rn2, rn3));
		prd.m_sampled_direction = Onb_to_world(onb, local_next_direction);
		prd.m_sampled_direction_pdf_w = brdf_cos_sampling_pdf;
    }
}
