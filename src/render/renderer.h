#pragma once

#include "core/vmath.h"
#include "passes/final_composite.h"
#include "passes/raytrace_visualize.h"
#include "render_context.h"
#include "render_params.h"
#include "rhi/rhi.h"
#include "scene_resource.h"

struct Renderer
{
    std::vector<Rhi::FramebufferBindings> m_raster_fbindings;
    std::array<Rhi::Texture, 2> m_rt_results;

    bool m_lskjdflksjdflkjsdlkfj = true;

    RaytraceVisualizePass m_pass_raytrace_visualize;
    BeautyPass m_pass_beauty;

    Renderer() {}

    void
    init(Rhi::Device * device, const int2 resolution, const std::vector<Rhi::Texture> & swapchain_attachment)
    {
        init_or_resize_resolution(device, resolution, swapchain_attachment);

        m_pass_raytrace_visualize = RaytraceVisualizePass(*device);
        m_pass_beauty             = BeautyPass(device);
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
        }
    }

    void
    loop(const RenderContext & ctx, const RenderParams & params)
    {
        Rhi::Device * device      = ctx.m_device;
        Rhi::CommandList cmd_list = ctx.m_graphics_command_pool->get_command_list();

        if (params.m_is_shaders_dirty || m_lskjdflksjdflkjsdlkfj)
        {
            m_pass_raytrace_visualize.init_or_reload(*device);
            m_pass_beauty.init_or_reload(*device, m_raster_fbindings[0]);

            m_lskjdflksjdflkjsdlkfj = false;
        }

        cmd_list.begin();

        // visualize
        m_pass_raytrace_visualize.run(cmd_list,
                                      ctx,
                                      params,
                                      m_rt_results[ctx.m_flight_index % 2],
                                      params.m_resolution);

        // transition
        cmd_list.transition_texture(m_rt_results[ctx.m_flight_index % 2],
                                    Rhi::TextureStateEnum::NonFragmentShaderVisible,
                                    Rhi::TextureStateEnum::FragmentShaderVisible);
        cmd_list.transition_texture(*ctx.m_swapchain_texture,
                                    Rhi::TextureStateEnum::Present,
                                    Rhi::TextureStateEnum::ColorAttachment);

        // run final pass
        m_pass_beauty.run(cmd_list,
                          ctx,
                          params,
                          m_rt_results[ctx.m_flight_index % 2],
                          m_raster_fbindings[ctx.m_image_index]);

        // render imgui onto swapchain
        if (params.m_should_imgui_drawn)
        {
            cmd_list.render_imgui(*ctx.m_imgui_render_pass, ctx.m_image_index);
        }

        // transition
        cmd_list.transition_texture(*ctx.m_swapchain_texture,
                                    Rhi::TextureStateEnum::ColorAttachment,
                                    Rhi::TextureStateEnum::Present);
        cmd_list.transition_texture(m_rt_results[ctx.m_flight_index % 2],
                                    Rhi::TextureStateEnum::FragmentShaderVisible,
                                    Rhi::TextureStateEnum::NonFragmentShaderVisible);

        cmd_list.end();
        cmd_list.submit(ctx.m_flight_fence, ctx.m_image_ready_semaphore, ctx.m_image_presentable_semaphore);
    }
};