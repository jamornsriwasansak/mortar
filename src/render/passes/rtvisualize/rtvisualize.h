#pragma once

#include "graphicsapi/graphicsapi.h"
#include "render/common/render_params.h"
#include "rtvisualize_shared.h"

struct RtVisualizePass
{
    Gp::RayTracingPipeline    m_rt_pipeline;
    Gp::RayTracingShaderTable m_rt_sbt;
    Gp::Buffer                m_cb_params;

    RtVisualizePass() {}

    RtVisualizePass(const Gp::Device * device)
    {
        std::filesystem::path shader_path = "../src/render/passes/rtvisualize/";

        // create pipeline for ssao
        Gp::RayTracingPipelineConfig rt_config;

        // raygen
        const Gp::ShaderSrc     raygen_shader(Gp::ShaderStageEnum::RayGen,
                                          shader_path / "rtvisualize.hlsl",
                                          "RayGen");
        [[maybe_unused]] size_t raygen_id = rt_config.add_shader(raygen_shader);

        // miss
        const Gp::ShaderSrc    miss_shader(Gp::ShaderStageEnum::Miss,
                                        shader_path / "rtvisualize.hlsl",
                                        "Miss");
        [[maybe_unused]] size_t miss_id = rt_config.add_shader(miss_shader);

        // hitgroup
        const Gp::ShaderSrc    hit_shader(Gp::ShaderStageEnum::ClosestHit,
                                       shader_path / "rtvisualize.hlsl",
                                       "ClosestHit");
        Gp::RayTracingHitGroup hit_group;
        hit_group.m_closest_hit_id = rt_config.add_shader(hit_shader);

        // all raygen and all hit groups
        [[maybe_unused]] size_t hitgroup_id = rt_config.add_hit_group(hit_group);

        m_rt_pipeline =
            Gp::RayTracingPipeline(device, rt_config, 16, 64, 2, "rt_visualize_pipeline");
        m_rt_sbt = Gp::RayTracingShaderTable(device, m_rt_pipeline, "rt_visualize_sbt");

        // constant params for rtvisualize
        m_cb_params = Gp::Buffer(device,
                                 Gp::BufferUsageEnum::ConstantBuffer,
                                 Gp::MemoryUsageEnum::CpuOnly,
                                 sizeof(RtVisualizeCbParams));
    }

    void
    run(Gp::CommandList &          cmd_list,
        const RenderContext &      render_ctx,
        const RenderParams &       render_params,
        const Gp::RayTracingTlas & tlas,
        const Gp::Texture &        target_texture_buffer,
        const uint2                target_resolution)
    {
        RtVisualizeCbParams cb_params;
        CameraProperties    cam_props = render_params.m_fps_camera->get_camera_props();
        cb_params.m_camera_inv_proj   = inverse(cam_props.m_proj);
        cb_params.m_camera_inv_view   = inverse(cam_props.m_view);
        std::memcpy(m_cb_params.map(), &cb_params, sizeof(RtVisualizeCbParams));
        m_cb_params.unmap();

        std::array<Gp::DescriptorSet, 1> descriptor_sets;
        descriptor_sets[0] =
            Gp::DescriptorSet(render_ctx.m_device, m_rt_pipeline, render_ctx.m_descriptor_pool, 0);
        descriptor_sets[0]
            .set_t_ray_tracing_accel(0, tlas)
            .set_u_rw_texture(0, target_texture_buffer)
            .set_b_constant_buffer(0, m_cb_params)
            .update();

        cmd_list.bind_raytrace_pipeline(m_rt_pipeline);
        cmd_list.bind_raytrace_descriptor_set(descriptor_sets);
        cmd_list.trace_rays(m_rt_sbt, target_resolution.x, target_resolution.y);
    }
};