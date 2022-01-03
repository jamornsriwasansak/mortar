#pragma once

#include "render/shader_path.h"
#include "rhi/rhi.h"
#include "shaders/cpp_compatible.h"
#include "shaders/path_tracing_params.h"

struct PathTracingPass
{
    Rhi::RayTracingPipeline    m_rt_pipeline;
    Rhi::RayTracingShaderTable m_rt_sbt;
    Rhi::Buffer                m_cb_params;

    PathTracingPass(const Rhi::Device & device, const ShaderBinaryManager & shader_binary_manager)
    : m_rt_pipeline("path_tracing_pipeline",
                    device,
                    ConstructGetRayTracePipelineConfig(),
                    shader_binary_manager,
                    sizeof(PathTracingAttributes),
                    sizeof(PathTracingPayload),
                    1),
      m_rt_sbt("path_tracing_sbt", device, m_rt_pipeline),
      m_cb_params("path_tracing_cb_params",
                  device,
                  Rhi::BufferUsageEnum::ConstantBuffer,
                  Rhi::MemoryUsageEnum::CpuToGpu,
                  sizeof(PathTracingCbParams))
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

        Rhi::RayTracingHitGroup       hit_group;
        [[maybe_unused]] const size_t raygen_id = rt_config.add_shader(raygen_shader);
        [[maybe_unused]] const size_t miss_id   = rt_config.add_shader(miss_shader);
        hit_group.m_closest_hit_id              = rt_config.add_shader(hit_shader);

        // all raygen and all hit groups
        [[maybe_unused]] const size_t hitgroup_id = rt_config.add_hit_group(hit_group);

        return rt_config;
    }

    void
    render(Rhi::CommandBuffer &  cmd_buffer,
           const RenderContext & ctx,
           const Rhi::Texture &  diffuse_direct_light_result,
           const Rhi::Texture &  depth_texture,
           const Rhi::Texture &  shading_normal_texture,
           const Rhi::Texture &  specular_roughness_texture,
           const uint2           target_resolution) const
    {
        // Setup params for Path Tracing pass
        PathTracingCbParams cb_params;
        CameraProperties    cam_props = ctx.m_fps_camera.get_camera_props();
        cb_params.m_camera_inv_proj   = inverse(cam_props.m_proj);
        cb_params.m_camera_inv_view   = inverse(cam_props.m_view);
        std::memcpy(m_cb_params.map(), &cb_params, sizeof(PathTracingCbParams));
        m_cb_params.unmap();

        // setup descriptor spaces and bindings
        std::array<Rhi::DescriptorSet, 2> descriptor_sets = {
            Rhi::DescriptorSet(ctx.m_device, m_rt_pipeline, ctx.m_per_flight_resource.m_descriptor_pool, 0),
            Rhi::DescriptorSet(ctx.m_device, m_rt_pipeline, ctx.m_per_flight_resource.m_descriptor_pool, 1)
        };

        PathTracingRegisters registers(descriptor_sets);
        registers.u_params.set(m_cb_params);
        registers.u_diffuse_direct_light_result.set(diffuse_direct_light_result);
        registers.u_gbuffer_depth.set(depth_texture);
        registers.u_gbuffer_shading_normal.set(shading_normal_texture);
        registers.u_scene_bvh.set(ctx.m_scene_resource.m_rt_tlas);

        descriptor_sets[0].update();
        descriptor_sets[1].update();

        cmd_buffer.bind_ray_trace_pipeline(m_rt_pipeline);
        cmd_buffer.bind_ray_trace_descriptor_set(descriptor_sets);
        cmd_buffer.trace_rays(m_rt_sbt, target_resolution.x, target_resolution.y);
    }
};
