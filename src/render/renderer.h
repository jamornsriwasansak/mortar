#pragma once

#include "core/vmath.h"
#include "passes/final_composite.h"
#include "passes/ray_trace_path_tracer.h"
#include "passes/ray_trace_visualize.h"
#include "profiler.h"
#include "render_context.h"
#include "rhi/rhi.h"
#include "scene_resource.h"

struct Renderer
{
    struct PerFlightRenderResource
    {
        Rhi::Texture m_rt_result;
        Profiler     m_profiler;

        static constexpr uint32_t NumMaxProfilerMarkers = 500;

        PerFlightRenderResource(const std::string & name, Rhi::Device & device, const int2 resolution)
        : m_profiler(name + "_profiler", device, NumMaxProfilerMarkers),
          m_rt_result(name + "_rt_result",
                      device,
                      Rhi::TextureUsageEnum::StorageImage | Rhi::TextureUsageEnum::ColorAttachment |
                          Rhi::TextureUsageEnum::Sampled,
                      Rhi::TextureStateEnum::NonFragmentShaderVisible,
                      Rhi::FormatEnum::R32G32B32A32_SFloat,
                      resolution,
                      nullptr,
                      nullptr,
                      float4(0.0f, 0.0f, 0.0f, 0.0f))
        {
        }
    };

    std::vector<Rhi::FramebufferBindings> m_raster_fbindings;
    std::vector<PerFlightRenderResource>  m_per_flight_resources;

    // all raytrace mode (rays start from the camera)
    enum class RayTraceMode : int
    {
        VisualizeDebug,
        PrimitivePathTracing
    };

    GuiEventCoordinator &   m_gui_event_coordinator;
    RayTraceMode            m_ray_trace_mode = RayTraceMode::VisualizeDebug;
    RayTraceVisualizeParams m_pass_ray_trace_visualize_params;
    RayTraceVisualizePass   m_pass_ray_trace_visualize;
    RayTracePathTracer      m_pass_ray_trace_primitive_pathtrace;
    RenderToFramebufferPass m_pass_render_to_framebuffer;

    Renderer(Rhi::Device &                               device,
             ShaderBinaryManager &                       shader_binary_manager,
             GuiEventCoordinator &                       gui_event_coordinator,
             const std::span<const Rhi::Texture * const> swapchain_attachment,
             const int2                                  resolution,
             const size_t                                num_flights)
    : m_raster_fbindings(construct_framebuffer_bindings(device, swapchain_attachment)),
      m_pass_ray_trace_visualize(device, shader_binary_manager),
      m_pass_ray_trace_primitive_pathtrace(device, shader_binary_manager),
      m_pass_render_to_framebuffer(device, shader_binary_manager, m_raster_fbindings[0]),
      m_per_flight_resources(construct_per_flight_resource(device, resolution, num_flights)),
      m_gui_event_coordinator(gui_event_coordinator)
    {
    }

    static std::vector<Rhi::FramebufferBindings>
    construct_framebuffer_bindings(const Rhi::Device &                           device,
                                   const std::span<const Rhi::Texture * const> & swapchain_attachment)
    {
        std::vector<Rhi::FramebufferBindings> result;
        result.reserve(swapchain_attachment.size());
        for (size_t i = 0; i < swapchain_attachment.size(); i++)
        {
            const std::vector<const Rhi::Texture *> attachments = { swapchain_attachment[i] };
            result.emplace_back("framebuffer_bindings", device, attachments);
        }
        return result;
    }

    static std::vector<PerFlightRenderResource>
    construct_per_flight_resource(Rhi::Device & device, const int2 resolution, const size_t num_flights)
    {
        std::vector<PerFlightRenderResource> result;
        result.reserve(num_flights);
        for (size_t i = 0; i < num_flights; i++)
        {
            result.emplace_back("flight_" + std::to_string(i), device, resolution);
        }
        return result;
    }

    void
    resize(Rhi::Device &                                 device,
           const int2                                    resolution,
           ShaderBinaryManager &                         shader_binary_manager,
           const std::span<const Rhi::Texture * const> & swapchain_attachments,
           const size_t                                  num_flights)
    {
        m_raster_fbindings     = construct_framebuffer_bindings(device, swapchain_attachments);
        m_per_flight_resources = construct_per_flight_resource(device, resolution, num_flights);
        m_pass_render_to_framebuffer.init_or_reload(device, shader_binary_manager, m_raster_fbindings[0]);
    }

    void
    loop(const RenderContext & ctx)
    {
        Rhi::CommandBuffer cmd_buffer =
            ctx.m_per_flight_resource.m_graphics_command_pool.get_command_buffer();
        PerFlightRenderResource & per_flight_render_resource = m_per_flight_resources[ctx.m_flight_index];

        Profiler & profiler = per_flight_render_resource.m_profiler;
        profiler.summarize();

        profiler.reset();

        if (ctx.m_is_shaders_dirty)
        {
            // init_or_reload_shader(ctx);
        }

        cmd_buffer.begin();

        if (ctx.m_do_profile)
        {
        }

        auto [begin_, end_] = profiler.get_new_profiling_interval_query_indices("yo");
        cmd_buffer.write_timestamp(per_flight_render_resource.m_profiler.m_query_pool, begin_);

        {
            // update all parameters
            m_pass_ray_trace_visualize_params.draw_gui(m_gui_event_coordinator);

            // raytrace mode
            bool p_open = true;
            if (m_gui_event_coordinator.m_display_main_pipeline_mode)
            {
                if (ImGui::Begin("Renderer Raytrace Mode", &m_gui_event_coordinator.m_display_main_pipeline_mode))
                {
                    ImGui::RadioButton("VisualizeDebug",
                                       reinterpret_cast<int *>(&m_ray_trace_mode),
                                       static_cast<int>(RayTraceMode::VisualizeDebug));
                    ImGui::RadioButton("PrimitivePathTracing",
                                       reinterpret_cast<int *>(&m_ray_trace_mode),
                                       static_cast<int>(RayTraceMode::PrimitivePathTracing));
                }
                ImGui::End();

                // visualize
                switch (m_ray_trace_mode)
                {
                case Renderer::RayTraceMode::VisualizeDebug:
                    m_pass_ray_trace_visualize.render(cmd_buffer,
                                                      ctx,
                                                      per_flight_render_resource.m_rt_result,
                                                      m_pass_ray_trace_visualize_params,
                                                      ctx.m_resolution);
                    break;
                case Renderer::RayTraceMode::PrimitivePathTracing:
                    m_pass_ray_trace_primitive_pathtrace.render(cmd_buffer,
                                                                ctx,
                                                                per_flight_render_resource.m_rt_result,
                                                                ctx.m_resolution);
                    break;
                default:
                    break;
                }
            }

            // transition
            cmd_buffer.transition_texture(per_flight_render_resource.m_rt_result,
                                          Rhi::TextureStateEnum::NonFragmentShaderVisible,
                                          Rhi::TextureStateEnum::FragmentShaderVisible);
            cmd_buffer.transition_texture(ctx.m_per_swap_resource.m_swapchain_texture,
                                          Rhi::TextureStateEnum::Present,
                                          Rhi::TextureStateEnum::ColorAttachment);

            // run final pass
            m_pass_render_to_framebuffer.run(cmd_buffer,
                                             ctx,
                                             per_flight_render_resource.m_rt_result,
                                             m_raster_fbindings[ctx.m_image_index]);

            // render imgui onto swapchain
            if (ctx.m_should_imgui_drawn)
            {
                cmd_buffer.render_imgui(ctx.m_imgui_render_pass, ctx.m_image_index);
            }

            // transition
            cmd_buffer.transition_texture(ctx.m_per_swap_resource.m_swapchain_texture,
                                          Rhi::TextureStateEnum::ColorAttachment,
                                          Rhi::TextureStateEnum::Present);
            cmd_buffer.transition_texture(per_flight_render_resource.m_rt_result,
                                          Rhi::TextureStateEnum::FragmentShaderVisible,
                                          Rhi::TextureStateEnum::NonFragmentShaderVisible);
        }

        cmd_buffer.write_timestamp(per_flight_render_resource.m_profiler.m_query_pool, end_);

        cmd_buffer.end();
        cmd_buffer.submit(&ctx.m_per_flight_resource.m_flight_fence,
                          &ctx.m_per_flight_resource.m_image_ready_semaphore,
                          &ctx.m_per_flight_resource.m_image_presentable_semaphore);
    }
};