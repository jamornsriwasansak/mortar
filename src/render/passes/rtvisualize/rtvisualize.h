#pragma once

#include "render/common/render_params.h"
#include "rhi/rhi.h"
#include "rtvisualize_shared.h"

struct RtVisualizePass
{
    Rhi::RayTracingPipeline m_rt_pipeline;
    Rhi::RayTracingShaderTable m_rt_sbt;
    Rhi::Buffer m_cb_params;
    Rhi::Sampler m_common_sampler;

    RtVisualizePass() {}

    RtVisualizePass(const Rhi::Device & device)
    {
        init_or_reload_shader(device);

        // constant params for rtvisualize
        m_cb_params = Rhi::Buffer(&device,
                                  Rhi::BufferUsageEnum::ConstantBuffer,
                                  Rhi::MemoryUsageEnum::CpuOnly,
                                  sizeof(RtVisualizeCbParams));

        // sampler
        m_common_sampler = Rhi::Sampler(&device);
    }

    void
    init_or_reload_shader(const Rhi::Device & device)
    {
        std::filesystem::path shader_path = "../src/render/passes/rtvisualize/";

        // create pipeline for ssao
        Rhi::RayTracingPipelineConfig rt_config;

        // raygen
        const Rhi::ShaderSrc raygen_shader(Rhi::ShaderStageEnum::RayGen,
                                           shader_path / "rtvisualize.hlsl",
                                           "RayGen");
        [[maybe_unused]] size_t raygen_id = rt_config.add_shader(raygen_shader);

        // miss
        const Rhi::ShaderSrc miss_shader(Rhi::ShaderStageEnum::Miss,
                                         shader_path / "rtvisualize.hlsl",
                                         "Miss");
        [[maybe_unused]] size_t miss_id = rt_config.add_shader(miss_shader);

        // hitgroup
        const Rhi::ShaderSrc hit_shader(Rhi::ShaderStageEnum::ClosestHit,
                                        shader_path / "rtvisualize.hlsl",
                                        "ClosestHit");
        Rhi::RayTracingHitGroup hit_group;
        hit_group.m_closest_hit_id = rt_config.add_shader(hit_shader);

        // all raygen and all hit groups
        [[maybe_unused]] size_t hitgroup_id = rt_config.add_hit_group(hit_group);

        m_rt_pipeline =
            Rhi::RayTracingPipeline(&device, rt_config, 16, 64, 2, "rt_visualize_pipeline");
        m_rt_sbt = Rhi::RayTracingShaderTable(&device, m_rt_pipeline, "rt_visualize_sbt");
    }

    void
    run(Rhi::CommandList & cmd_list,
        const RenderContext & render_ctx,
        const RenderParams & render_params,
        const Rhi::RayTracingTlas & tlas,
        const Rhi::Texture & target_texture_buffer,
        const uint2 target_resolution)
    {
        bool p_open           = true;
        static int rtvis_mode = 0;
        if (ImGui::Begin("RtVisualize Pass", &p_open))
        {
            ImGui::RadioButton("InstanceId", &rtvis_mode, RtVisualizeCbParams::ModeInstanceId);
            ImGui::RadioButton("GeometryId", &rtvis_mode, RtVisualizeCbParams::ModeGeometryId);
            ImGui::RadioButton("TriangleId", &rtvis_mode, RtVisualizeCbParams::ModeTriangleId);
            ImGui::RadioButton("BaryCentricCoords", &rtvis_mode, RtVisualizeCbParams::ModeBaryCentricCoords);
            ImGui::RadioButton("Position", &rtvis_mode, RtVisualizeCbParams::ModePosition);
            ImGui::RadioButton("Geometry Normal", &rtvis_mode, RtVisualizeCbParams::ModeGeometryNormal);
            ImGui::RadioButton("Texture Coord", &rtvis_mode, RtVisualizeCbParams::ModeSpecularReflectance);
            ImGui::RadioButton("Depth", &rtvis_mode, RtVisualizeCbParams::ModeDepth);
            ImGui::RadioButton("DiffuseReflectance", &rtvis_mode, RtVisualizeCbParams::ModeDiffuseReflectance);
            ImGui::RadioButton("SpecularReflectance", &rtvis_mode, RtVisualizeCbParams::ModeSpecularReflectance);
        }
        ImGui::End();

        RtVisualizeCbParams cb_params;
        CameraProperties cam_props  = render_params.m_fps_camera->get_camera_props();
        cb_params.m_camera_inv_proj = inverse(cam_props.m_proj);
        cb_params.m_camera_inv_view = inverse(cam_props.m_view);
        cb_params.m_mode            = rtvis_mode;
        std::memcpy(m_cb_params.map(), &cb_params, sizeof(RtVisualizeCbParams));
        m_cb_params.unmap();

        // setup descriptor spaces and bindings
        std::array<Rhi::DescriptorSet, 2> descriptor_sets;

        // descriptor sets 0
        descriptor_sets[0] =
            Rhi::DescriptorSet(render_ctx.m_device, m_rt_pipeline, render_ctx.m_descriptor_pool, 0);
        descriptor_sets[0]
            .set_t_ray_tracing_accel(0, render_params.m_scene->m_rt_tlas)
            .set_u_rw_texture(0, target_texture_buffer)
            .set_b_constant_buffer(0, m_cb_params)
            .update();

        // descriptor sets 1
        descriptor_sets[1] =
            Rhi::DescriptorSet(render_ctx.m_device, m_rt_pipeline, render_ctx.m_descriptor_pool, 1);
        descriptor_sets[1].set_s_sampler(0, m_common_sampler);
        for (size_t i = 0; i < render_params.m_scene->m_d_textures.length(); i++)
        {
            descriptor_sets[1].set_t_texture(0, render_params.m_scene->m_d_textures[i], i);
        }
        descriptor_sets[1].set_b_constant_buffer(0, render_params.m_scene->m_d_materials);
        descriptor_sets[1].update();

        cmd_list.bind_raytrace_pipeline(m_rt_pipeline);
        cmd_list.bind_raytrace_descriptor_set(descriptor_sets);
        cmd_list.trace_rays(m_rt_sbt, target_resolution.x, target_resolution.y);
    }
};