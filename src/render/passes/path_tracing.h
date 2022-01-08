#pragma once

#include "render/shader_path.h"
#include "rhi/rhi.h"
#include "shaders/cpp_compatible.h"
#include "shaders/path_tracing_params.h"

struct PathTracingPass
{
    Rhi::RayTracingPipeline    m_rt_pipeline;
    Rhi::RayTracingShaderTable m_rt_sbt;
    std::vector<Rhi::Buffer>   m_params_constant_buffers;
    Rhi::Sampler               m_common_sampler;
    size_t                     m_radiance_miss_shader_index;
    size_t                     m_shadow_miss_shader_index;

    PathTracingPass(const Rhi::Device & device, const ShaderBinaryManager & shader_binary_manager, const size_t num_flights)
    : m_rt_pipeline("path_tracing_pipeline",
                    device,
                    ConstructGetRayTracePipelineConfig(),
                    shader_binary_manager,
                    sizeof(PathTracingAttributes),
                    std::max(sizeof(PathTracingPayload), sizeof(PathTracingShadowRayPayload)),
                    1),
      m_rt_sbt("path_tracing_sbt", device, m_rt_pipeline),
      m_params_constant_buffers(ConstructParamsConstantBuffers(device, num_flights)),
      m_common_sampler("path_tracing_sampler", device)
    {
    }

    static Rhi::RayTracingPipelineConfig
    ConstructGetRayTracePipelineConfig()
    {
        // create pipeline
        Rhi::RayTracingPipelineConfig rt_config;

        // raygen
        const Rhi::ShaderSrc raygen_shader(Rhi::ShaderStageEnum::RayGen,
                                           BASE_SHADER_DIR "path_tracing.hlsl.h",
                                           "RayGen");

        // miss
        const Rhi::ShaderSrc miss_shader(Rhi::ShaderStageEnum::Miss,
                                         BASE_SHADER_DIR "path_tracing.hlsl.h",
                                         "Miss");

        // hitgroup
        const Rhi::ShaderSrc hit_shader(Rhi::ShaderStageEnum::ClosestHit,
                                        BASE_SHADER_DIR "path_tracing.hlsl.h",
                                        "ClosestHit");

        // shadow miss
        const Rhi::ShaderSrc shadow_miss_shader(Rhi::ShaderStageEnum::Miss,
                                                BASE_SHADER_DIR "path_tracing.hlsl.h",
                                                "ShadowMiss");

        Rhi::RayTracingHitGroup       hit_group;
        [[maybe_unused]] const size_t raygen_id      = rt_config.add_shader(raygen_shader);
        [[maybe_unused]] const size_t miss_id        = rt_config.add_shader(miss_shader);
        [[maybe_unused]] const size_t shadow_miss_id = rt_config.add_shader(shadow_miss_shader);
        hit_group.m_closest_hit_id                   = rt_config.add_shader(hit_shader);

        // all raygen and all hit groups
        [[maybe_unused]] const size_t hitgroup_id = rt_config.add_hit_group(hit_group);

        return rt_config;
    }

    static std::vector<Rhi::Buffer>
    ConstructParamsConstantBuffers(const Rhi::Device & device, const size_t num_flights)
    {
        std::vector<Rhi::Buffer> result;
        result.reserve(num_flights);

        for (size_t i_flight = 0; i_flight < num_flights; i_flight++)
        {
            result.emplace_back("path_tracing_params_constant_buffer_" + std::to_string(i_flight),
                                device,
                                Rhi::BufferUsageEnum::ConstantBuffer,
                                Rhi::MemoryUsageEnum::CpuToGpu,
                                sizeof(PathTracingCbParams));
        }

        return result;
    }

    void
    render(Rhi::CommandBuffer &  cmd_buffer,
           const RenderContext & ctx,
           const Rhi::Texture &  demodulated_diffuse_gi,
           const Rhi::Texture &  gbuffer_depth,
           const Rhi::Texture &  gbuffer_shading_normal,
           const Rhi::Texture &  diffuse_reflectance_texture,
           const Rhi::Texture &  specular_reflectance_texture,
           const Rhi::Texture &  specular_roughness_texture,
           const uint2           target_resolution) const
    {
        // Setup params for Path Tracing pass
        PathTracingCbParams cb_params;
        CameraProperties    cam_props              = ctx.m_fps_camera.get_camera_props();
        cb_params.m_camera_inv_proj                = inverse(cam_props.m_proj);
        cb_params.m_camera_inv_view                = inverse(cam_props.m_view);
        const Rhi::Buffer & params_constant_buffer = m_params_constant_buffers[ctx.m_flight_index];
        std::memcpy(params_constant_buffer.map(), &cb_params, sizeof(PathTracingCbParams));
        params_constant_buffer.unmap();

        // setup descriptor spaces and bindings
        std::array<Rhi::DescriptorSet, 2> descriptor_sets = {
            Rhi::DescriptorSet(ctx.m_device, m_rt_pipeline, ctx.m_per_flight_resource.m_descriptor_pool, 0),
            Rhi::DescriptorSet(ctx.m_device, m_rt_pipeline, ctx.m_per_flight_resource.m_descriptor_pool, 1)
        };

        PathTracingRegisters registers(descriptor_sets);
        registers.u_params.set(params_constant_buffer);
        registers.u_demodulated_diffuse_gi.set(demodulated_diffuse_gi);
        registers.u_gbuffer_depth.set(gbuffer_depth);
        registers.u_gbuffer_shading_normal.set(gbuffer_shading_normal);
        registers.u_gbuffer_diffuse_reflectance.set(diffuse_reflectance_texture);
        registers.u_gbuffer_specular_reflectance.set(specular_reflectance_texture);
        registers.u_gbuffer_roughness.set(specular_roughness_texture);

        registers.u_scene_bvh.set(ctx.m_scene_resource.m_rt_tlas);
        registers.u_sampler.set(m_common_sampler);
        registers.u_base_instance_table.set(ctx.m_scene_resource.m_d_base_instance_table);
        registers.u_geometry_table.set(ctx.m_scene_resource.m_d_geometry_table);
        registers.u_indices.set(ctx.m_scene_resource.m_d_ibuf);
        registers.u_compact_vertices.set(ctx.m_scene_resource.m_d_vbuf_packed);
        registers.u_materials.set(ctx.m_scene_resource.m_d_materials);
        for (size_t i = 0; i < ctx.m_scene_resource.m_d_textures.size(); i++)
        {
            registers.u_textures.set(ctx.m_scene_resource.m_d_textures[i], i);
        }

        descriptor_sets[0].update();
        descriptor_sets[1].update();

        cmd_buffer.bind_ray_trace_pipeline(m_rt_pipeline);
        cmd_buffer.bind_ray_trace_descriptor_set(descriptor_sets);
        cmd_buffer.trace_rays(m_rt_sbt, target_resolution.x, target_resolution.y);
    }
};
