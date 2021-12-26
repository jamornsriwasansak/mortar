#pragma once

#include "core/vmath.h"
#include "gpu_profiler.h"
#include "passes/final_composite.h"
#include "passes/gbuffer_generation.h"
#include "render_context.h"
#include "rhi/rhi.h"
#include "scene_resource.h"

struct RendererDebugResource
{
    GpuProfilerGui            m_gpu_profiler_gui;
    std::vector<GpuProfiler>  m_gpu_profilers;
    static constexpr uint32_t NumMaxProfilerMarkers = 500;

    RendererDebugResource(Rhi::Device & device, const size_t num_flights)
    : m_gpu_profilers(construct_per_flight_gpu_profilers(device, num_flights)), m_gpu_profiler_gui(num_flights)
    {
    }

    static std::vector<GpuProfiler>
    construct_per_flight_gpu_profilers(Rhi::Device & device, const size_t num_flights)
    {
        std::vector<GpuProfiler> result;
        result.reserve(num_flights);
        for (size_t i = 0; i < num_flights; i++)
        {
            result.emplace_back("gpu_profiler_" + std::to_string(i), device, NumMaxProfilerMarkers);
        }
        return result;
    }
};

struct Renderer
{
    struct PerFlightRenderResource
    {
        Rhi::Texture m_rt_result;

        PerFlightRenderResource(const std::string & name, Rhi::Device & device, const int2 resolution)
        : m_rt_result(name + "_rt_result",
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

    // All raytrace mode (rays start from the camera)
    enum class RayTraceMode : int
    {
        VisualizeDebug,
        PrimitivePathTracing
    };

    GuiEventCoordinator &       m_gui_event_coordinator;
    RayTraceMode                m_ray_trace_mode = RayTraceMode::VisualizeDebug;
    RayTraceVisualizeUiParams   m_pass_ray_trace_visualize_params;
    RayTraceGbufferGeneratePass m_pass_ray_trace_gbuffer_generate;
    RenderToFramebufferPass     m_pass_render_to_framebuffer;

    std::unique_ptr<RendererDebugResource> m_debug_resource;

    Renderer(Rhi::Device &                               device,
             ShaderBinaryManager &                       shader_binary_manager,
             GuiEventCoordinator &                       gui_event_coordinator,
             const std::span<const Rhi::Texture * const> swapchain_attachment,
             const int2                                  resolution,
             const size_t                                num_flights)
    : m_raster_fbindings(construct_framebuffer_bindings(device, swapchain_attachment)),
      m_pass_ray_trace_gbuffer_generate(device, shader_binary_manager),
      m_pass_render_to_framebuffer(device, shader_binary_manager, m_raster_fbindings[0]),
      m_per_flight_resources(construct_per_flight_resource(device, resolution, num_flights)),
      m_gui_event_coordinator(gui_event_coordinator),
#ifndef PRODUCT
      m_debug_resource(std::make_unique<RendererDebugResource>(device, num_flights))
#endif
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
        // Display the gui for params and human readable data
        m_pass_ray_trace_visualize_params.draw_gui(m_gui_event_coordinator);
        if (m_debug_resource)
        {
            m_debug_resource->m_gpu_profiler_gui.draw_gui(m_gui_event_coordinator);
        }

        PerFlightRenderResource & per_flight_render_resource = m_per_flight_resources[ctx.m_flight_index];

        GpuProfiler * gpu_profiler = nullptr;
        if (m_debug_resource && !m_debug_resource->m_gpu_profiler_gui.m_pause)
        {
            // Summarize the information recorded in the gpu profiler
            gpu_profiler = &m_debug_resource->m_gpu_profilers[ctx.m_flight_index];
            gpu_profiler->summarize();

            // Show the result of gpu profiler in a human readable format
            m_debug_resource->m_gpu_profiler_gui.update(gpu_profiler->m_profiling_intervals,
                                                        gpu_profiler->m_ns_from_timestamp);

            // Reset gpu profiler
            gpu_profiler->reset();
        }

        // Begin recording command buffer for rendering
        Rhi::CommandBuffer cmd_buffer =
            ctx.m_per_flight_resource.m_graphics_command_pool.get_command_buffer();
        cmd_buffer.begin();

        {
            GpuProfilingScope rendering("Rendering", cmd_buffer, gpu_profiler);

            // Generate gbuffer
            {
                GpuProfilingScope gbuffer_scope("Generate gbuffer", cmd_buffer, gpu_profiler);
                m_pass_ray_trace_gbuffer_generate.render(cmd_buffer,
                                                         ctx,
                                                         per_flight_render_resource.m_rt_result,
                                                         m_pass_ray_trace_visualize_params,
                                                         ctx.m_resolution);
            }

            // Transition
            {
                GpuProfilingScope transit_scope("Transit present to color attachment", cmd_buffer, gpu_profiler);
                cmd_buffer.transition_texture(per_flight_render_resource.m_rt_result,
                                              Rhi::TextureStateEnum::NonFragmentShaderVisible,
                                              Rhi::TextureStateEnum::FragmentShaderVisible);
                cmd_buffer.transition_texture(ctx.m_per_swap_resource.m_swapchain_texture,
                                              Rhi::TextureStateEnum::Present,
                                              Rhi::TextureStateEnum::ColorAttachment);
            }

            // Run final pass
            {
                GpuProfilingScope render_to_framebuffer_scope("Render rtresult to framebuffer", cmd_buffer, gpu_profiler);
                m_pass_render_to_framebuffer.run(cmd_buffer,
                                                 ctx,
                                                 per_flight_render_resource.m_rt_result,
                                                 m_raster_fbindings[ctx.m_image_index]);
            }

            // Render imgui onto swapchain
            if (ctx.m_should_imgui_drawn)
            {
                GpuProfilingScope imgui_scope("Imgui", cmd_buffer, gpu_profiler);
                cmd_buffer.render_imgui(ctx.m_imgui_render_pass, ctx.m_image_index);
            }

            // Transition
            {
                GpuProfilingScope transit_scope("Transit swapchain for present", cmd_buffer, gpu_profiler);
                cmd_buffer.transition_texture(ctx.m_per_swap_resource.m_swapchain_texture,
                                              Rhi::TextureStateEnum::ColorAttachment,
                                              Rhi::TextureStateEnum::Present);
                cmd_buffer.transition_texture(per_flight_render_resource.m_rt_result,
                                              Rhi::TextureStateEnum::FragmentShaderVisible,
                                              Rhi::TextureStateEnum::NonFragmentShaderVisible);
            }
        }

        // End recording
        cmd_buffer.end();
        cmd_buffer.submit(&ctx.m_per_flight_resource.m_flight_fence,
                          &ctx.m_per_flight_resource.m_image_ready_semaphore,
                          &ctx.m_per_flight_resource.m_image_presentable_semaphore);
    }
};