#pragma once

#include "rhi/rhi.h"
#include "shaders/ray_trace_path_tracer_params.h"

struct RayTracePathTracer
{
    Rhi::RayTracingPipeline    m_rt_pipeline;
    Rhi::RayTracingShaderTable m_rt_sbt;
    Rhi::Buffer                m_cb_params;
    Rhi::Sampler               m_common_sampler;

    RayTracePathTracer(const Rhi::Device & device, const ShaderBinaryManager & shader_binary_manager)
    : m_cb_params("ray_trace_primitive_path_tracer_cb_params",
                  device,
                  Rhi::BufferUsageEnum::ConstantBuffer,
                  Rhi::MemoryUsageEnum::CpuToGpu,
                  sizeof(RaytracePathTracingCbParams)),
      m_common_sampler("ray_trace_path_trace_sampler", device),
      m_rt_pipeline("ray_trace_path_trace_pipeline", device, construct_rt_config(), shader_binary_manager, 16, 64, 2),
      m_rt_sbt("raytrace_visualize_sbt", device, m_rt_pipeline)
    {
    }

    Rhi::RayTracingPipelineConfig
    construct_rt_config() const
    {
        // create pipeline for ssao
        Rhi::RayTracingPipelineConfig rt_config;

        // raygen
        const Rhi::ShaderSrc          raygen_shader(Rhi::ShaderStageEnum::RayGen,
                                           BASE_SHADER_DIR "ray_trace_path_tracer.hlsl",
                                           "RayGen");
        [[maybe_unused]] const size_t raygen_id = rt_config.add_shader(raygen_shader);

        // miss
        const Rhi::ShaderSrc          miss_shader(Rhi::ShaderStageEnum::Miss,
                                         BASE_SHADER_DIR "ray_trace_path_tracer.hlsl",
                                         "Miss");
        [[maybe_unused]] const size_t miss_id = rt_config.add_shader(miss_shader);

        // hitgroup
        const Rhi::ShaderSrc    hit_shader(Rhi::ShaderStageEnum::ClosestHit,
                                        BASE_SHADER_DIR "ray_trace_path_tracer.hlsl",
                                        "ClosestHit");
        Rhi::RayTracingHitGroup hit_group;
        hit_group.m_closest_hit_id = rt_config.add_shader(hit_shader);

        // all raygen and all hit groups
        [[maybe_unused]] const size_t hitgroup_id = rt_config.add_hit_group(hit_group);

        return rt_config;
    }

    void
    draw_gui()
    {
    }

    void
    render(Rhi::CommandBuffer &  cmd_buffer,
           const RenderContext & ctx,
           const Rhi::Texture &  target_texture_buffer,
           const uint2           target_resolution)
    {
        RaytracePathTracingCbParams cb_params;
        CameraProperties            cam_props = ctx.m_fps_camera.get_camera_props();
        cb_params.m_camera_inv_proj           = inverse(cam_props.m_proj);
        cb_params.m_camera_inv_view           = inverse(cam_props.m_view);
        std::memcpy(m_cb_params.map(), &cb_params, sizeof(RaytracePathTracingCbParams));
        m_cb_params.unmap();

        std::array<Rhi::DescriptorSet, 2> descriptor_sets{
            Rhi::DescriptorSet(ctx.m_device, m_rt_pipeline, ctx.m_per_flight_resource.m_descriptor_pool, 0),
            Rhi::DescriptorSet(ctx.m_device, m_rt_pipeline, ctx.m_per_flight_resource.m_descriptor_pool, 1)
        };

        // descriptor sets 0
        descriptor_sets[0]
            .set_u_rw_texture(0, target_texture_buffer)
            .set_b_constant_buffer(0, m_cb_params)
            .update();

        // descriptor sets 1
        for (size_t i = 0; i < std::min(ctx.m_scene_resource.m_d_textures.size(),
                                        size_t(EngineSetting::MaxNumBindlessTextures));
             i++)
        {
            descriptor_sets[1].set_t_texture(7, ctx.m_scene_resource.m_d_textures[i], i);
        }
        for (size_t i = ctx.m_scene_resource.m_d_textures.size(); i < EngineSetting::MaxNumBindlessTextures; i++)
        {
            descriptor_sets[1].set_t_texture(7, ctx.m_scene_resource.m_d_textures[0], i);
        }
        descriptor_sets[1]
            .set_s_sampler(0, m_common_sampler)
            .set_t_ray_tracing_accel(0, ctx.m_scene_resource.m_rt_tlas)
            .set_t_structured_buffer(1,
                                     ctx.m_scene_resource.m_d_base_instance_table,
                                     sizeof(BaseInstanceTableEntry),
                                     ctx.m_scene_resource.m_num_base_instance_table_entries)
            .set_t_structured_buffer(2,
                                     ctx.m_scene_resource.m_d_geometry_table,
                                     sizeof(GeometryTableEntry),
                                     ctx.m_scene_resource.m_num_geometry_table_entries)
            .set_t_structured_buffer(3,
                                     ctx.m_scene_resource.m_d_ibuf,
                                     sizeof(uint16_t),
                                     ctx.m_scene_resource.m_num_indices)
            .set_t_structured_buffer(4,
                                     ctx.m_scene_resource.m_d_vbuf_position,
                                     sizeof(float3),
                                     ctx.m_scene_resource.m_num_vertices)
            .set_t_structured_buffer(5,
                                     ctx.m_scene_resource.m_d_vbuf_packed,
                                     sizeof(CompactVertex),
                                     ctx.m_scene_resource.m_num_vertices)
            .set_t_structured_buffer(6, ctx.m_scene_resource.m_d_materials, sizeof(StandardMaterial), EngineSetting::MaxNumStandardMaterials)
            .update();

        cmd_buffer.bind_ray_trace_pipeline(m_rt_pipeline);
        cmd_buffer.bind_ray_trace_descriptor_set(descriptor_sets);
        cmd_buffer.trace_rays(m_rt_sbt, target_resolution.x, target_resolution.y);
    }
};
