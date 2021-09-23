#pragma once

#include "common/camera_params.h"
#include "common/render_params.h"
#include "passes/restir_direct_light/bindless_object_table.h"
#include "passes/restir_direct_light/direct_light_params.h"
#include "passes/restir_direct_light/reservior.h"
#include "passes/rtvisualize/rtvisualize.h"
#include "rendercontext.h"
#include "rhi/rhi.h"
#include "scene_resource.h"

struct Renderer
{
    Rhi::RasterPipeline m_raster_pipeline;
    Rhi::RayTracingPipeline m_rt_pipeline;
    Rhi::RayTracingShaderTable m_rt_sbt;
    std::vector<Rhi::FramebufferBindings> m_raster_fbindings;
    std::array<Rhi::Texture, 2> m_rt_results;
    std::array<Rhi::Buffer, 2> m_prev_frame_reserviors;
    Rhi::Sampler m_sampler;

    RtVisualizePass m_pass_rt_visualize;

    Renderer() {}

    void
    init(Rhi::Device * device, const int2 resolution, const std::vector<Rhi::Texture> & swapchain_attachment)
    {
        m_sampler = Rhi::Sampler(device);

        init_or_resize_resolution(device, resolution, swapchain_attachment);
        init_shaders(device, true);

        m_pass_rt_visualize = RtVisualizePass(*device);
    }

    void
    init_or_resize_resolution(Rhi::Device * device, const int2 resolution, const std::vector<Rhi::Texture> & swapchain_attachment)
    {
        m_raster_fbindings.resize(swapchain_attachment.size());
        for (size_t i = 0; i < swapchain_attachment.size(); i++)
        {
            m_raster_fbindings[i] = Rhi::FramebufferBindings(device, { &swapchain_attachment[i] });
        }

        for (size_t i = 0; i < 2; i++)
        {
            m_rt_results[i] =
                Rhi::Texture(device,
                             Rhi::TextureUsageEnum::StorageImage |
                                 Rhi::TextureUsageEnum::ColorAttachment | Rhi::TextureUsageEnum::Sampled,
                             Rhi::TextureStateEnum::NonFragmentShaderVisible,
                             Rhi::FormatEnum::R32G32B32A32_SFloat,
                             resolution,
                             nullptr,
                             nullptr,
                             float4(0.0f, 0.0f, 0.0f, 0.0f),
                             "ray_tracing_result");
            m_prev_frame_reserviors[i] = Rhi::Buffer(device,
                                                     Rhi::BufferUsageEnum::StorageBuffer,
                                                     Rhi::MemoryUsageEnum::GpuOnly,
                                                     sizeof(Reservior) * resolution.x * resolution.y,
                                                     nullptr,
                                                     nullptr,
                                                     "prev_frame_reservior");
        }
    }

    void
    init_shaders(Rhi::Device * device, const bool is_first_time)
    {
        // raster pipeline
        try
        {
            m_raster_pipeline = [&]() {
                std::filesystem::path shader_path = "../src/render/passes/beauty/";
                Rhi::ShaderSrc vertexShaderSrc2(Rhi::ShaderStageEnum::Vertex);
                vertexShaderSrc2.m_entry     = "VsMain";
                vertexShaderSrc2.m_file_path = shader_path / "beauty.hlsl";

                Rhi::ShaderSrc fragmentShaderSrc2(Rhi::ShaderStageEnum::Fragment);
                fragmentShaderSrc2.m_entry     = "FsMain";
                fragmentShaderSrc2.m_file_path = shader_path / "beauty.hlsl";

                std::vector<Rhi::ShaderSrc> ssao_shader_srcs;
                ssao_shader_srcs.push_back(vertexShaderSrc2);
                ssao_shader_srcs.push_back(fragmentShaderSrc2);

                return Rhi::RasterPipeline(device, ssao_shader_srcs, m_raster_fbindings[0]);
            }();

            // raytracing pipeline
            m_rt_pipeline = [&]() {
                std::filesystem::path shader_path = "../src/render/passes/restir_direct_light/";

                // create pipeline for ssao
                Rhi::ShaderSrc raygen_shader(Rhi::ShaderStageEnum::RayGen);
                raygen_shader.m_entry     = "RayGen";
                raygen_shader.m_file_path = shader_path / "direct_light.h";

                Rhi::ShaderSrc standard_hit_shader(Rhi::ShaderStageEnum::ClosestHit);
                standard_hit_shader.m_entry     = "ClosestHit";
                standard_hit_shader.m_file_path = shader_path / "direct_light.h";

                Rhi::ShaderSrc emissive_hit_shader(Rhi::ShaderStageEnum::ClosestHit);
                emissive_hit_shader.m_entry     = "EmissiveClosestHit";
                emissive_hit_shader.m_file_path = shader_path / "direct_light.h";

                Rhi::ShaderSrc miss_shader(Rhi::ShaderStageEnum::Miss);
                miss_shader.m_entry     = "Miss";
                miss_shader.m_file_path = shader_path / "direct_light.h";

                Rhi::ShaderSrc shadow_miss_shader(Rhi::ShaderStageEnum::Miss);
                shadow_miss_shader.m_entry     = "ShadowMiss";
                shadow_miss_shader.m_file_path = shader_path / "direct_light.h";

                Rhi::RayTracingPipelineConfig rt_config;

                [[maybe_unused]] size_t raygen_id = rt_config.add_shader(raygen_shader);
                [[maybe_unused]] size_t miss_id   = rt_config.add_shader(miss_shader);
                [[maybe_unused]] size_t miss_id2  = rt_config.add_shader(miss_shader);

                size_t closesthit_id                = rt_config.add_shader(standard_hit_shader);
                [[maybe_unused]] size_t hitgroup_id = rt_config.add_hit_group(closesthit_id);

                size_t emissive_closesthit_id = rt_config.add_shader(emissive_hit_shader);
                [[maybe_unused]] size_t emisive_hitgroup_id = rt_config.add_hit_group(emissive_closesthit_id);

                return Rhi::RayTracingPipeline(device, rt_config, 16, 64, 2, "raytracing_pipeline");
            }();

            // raytracing visualize pipeline

            m_rt_sbt = Rhi::RayTracingShaderTable(device, m_rt_pipeline, "raytracing_shader_table");
        }
        catch (const std::exception & e)
        {
            if (is_first_time)
            {
                throw e;
            }
            else
            {
                Logger::Info(__FUNCTION__, " cannot reload shader due to ", e.what());
            }
        }
    }

    int frame_count = 0;
    void
    loop(const RenderContext & ctx, const RenderParams & params)
    {
        Rhi::Device * device  = ctx.m_device;
        Rhi::CommandList cmds = ctx.m_graphics_command_pool->get_command_list();

        if (params.m_is_shaders_dirty)
        {
            init_shaders(device, false);
            m_pass_rt_visualize.init_or_reload_shader(*device);
        }

        cmds.begin();

        // do raytracing
        cmds.bind_raytrace_pipeline(m_rt_pipeline);

        // set ray desc
        std::array<Rhi::DescriptorSet, 4> ray_descriptor_sets;
        ray_descriptor_sets[0] =
            Rhi::DescriptorSet(device, m_rt_pipeline, ctx.m_descriptor_pool, 0, "rt_desc_set0");
        ray_descriptor_sets[1] =
            Rhi::DescriptorSet(device, m_rt_pipeline, ctx.m_descriptor_pool, 1, "rt_desc_set1");
        ray_descriptor_sets[2] =
            Rhi::DescriptorSet(device, m_rt_pipeline, ctx.m_descriptor_pool, 2, "rt_desc_set2");
        ray_descriptor_sets[3] =
            Rhi::DescriptorSet(device, m_rt_pipeline, ctx.m_descriptor_pool, 3, "rt_desc_set3");

        // set camera uniform
        auto transform = params.m_fps_camera->get_camera_props();
        CameraParams cam_params;
        cam_params.m_inv_view = inverse(transform.m_view);
        cam_params.m_inv_proj = inverse(transform.m_proj);

        // ray tracing pass
        // cmds.bind_raytrace_descriptor_set(ray_descriptor_sets);
        // cmds.trace_rays(m_rt_sbt, params.m_resolution.x, params.m_resolution.y);

        m_pass_rt_visualize
            .run(cmds, ctx, params, m_rt_results[ctx.m_flight_index % 2], params.m_resolution);

        {
            // transition
            cmds.transition_texture(m_rt_results[ctx.m_flight_index % 2],
                                    Rhi::TextureStateEnum::NonFragmentShaderVisible,
                                    Rhi::TextureStateEnum::FragmentShaderVisible);
            cmds.transition_texture(*ctx.m_swapchain_texture,
                                    Rhi::TextureStateEnum::Present,
                                    Rhi::TextureStateEnum::ColorAttachment);

            // draw result
            cmds.begin_render_pass(m_raster_fbindings[ctx.m_image_index]);
            {
                cmds.bind_raster_pipeline(m_raster_pipeline);

                std::array<Rhi::DescriptorSet, 1> beauty_desc_sets;
                beauty_desc_sets[0] =
                    Rhi::DescriptorSet(device, m_raster_pipeline, ctx.m_descriptor_pool, 0);
                beauty_desc_sets[0]
                    .set_t_texture(0, m_rt_results[ctx.m_flight_index % 2])
                    .set_s_sampler(0, m_sampler)
                    .update();

                // raster
                cmds.bind_graphics_descriptor_set(beauty_desc_sets);
                cmds.bind_vertex_buffer(params.m_scene_resource->m_d_materials, sizeof(CompactVertex));
                cmds.bind_index_buffer(params.m_scene_resource->m_d_ibuf, Rhi::IndexType::Uint32);
                cmds.draw_instanced(3, 1, 0, 0);
            }
            cmds.end_render_pass();

            // render imgui onto swapchain
            if (params.m_should_imgui_drawn)
            {
                cmds.render_imgui(*ctx.m_imgui_render_pass, ctx.m_image_index);
            }

            // transition
            cmds.transition_texture(*ctx.m_swapchain_texture,
                                    Rhi::TextureStateEnum::ColorAttachment,
                                    Rhi::TextureStateEnum::Present);
            cmds.transition_texture(m_rt_results[ctx.m_flight_index % 2],
                                    Rhi::TextureStateEnum::FragmentShaderVisible,
                                    Rhi::TextureStateEnum::NonFragmentShaderVisible);

            cmds.end();
            cmds.submit(ctx.m_flight_fence, ctx.m_image_ready_semaphore, ctx.m_image_presentable_semaphore);
        }
    }
};