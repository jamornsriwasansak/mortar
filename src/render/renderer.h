#pragma once

#include "core/vmath.h"
#include "gpu_profiler.h"
#include "passes/final_composite.h"
#include "passes/gbuffer_generation.h"
#include "passes/path_tracing.h"
#include "render_context.h"
#include "rhi/rhi.h"
#include "scene_resource.h"

struct Renderer
{
    struct PerFlightRenderResource
    {
        GpuProfiler m_gpu_profiler;

        // GBuffer
        Rhi::Texture m_depth_texture;
        Rhi::Texture m_shading_normal_texture;
        Rhi::Texture m_diffuse_reflectance_texture;
        Rhi::Texture m_specular_reflectance_texture;
        Rhi::Texture m_specular_roughness_texture;
        Rhi::Texture m_diffuse_direct_result_texture;

        static constexpr uint32_t NumMaxProfilerMarkers = 500;

        PerFlightRenderResource(const std::string & name, Rhi::Device & device, const int2 resolution)
        : m_gpu_profiler(name + "_gpu_profiler", device, NumMaxProfilerMarkers),
          m_depth_texture(name + "_gbuffer_depth_texture",
                          device,
                          Rhi::TextureCreateInfo(resolution.x,
                                                 resolution.y,
                                                 Rhi::FormatEnum::R32_SFloat,
                                                 Rhi::TextureUsageEnum::StorageImage |
                                                     Rhi::TextureUsageEnum::ColorAttachment),
                          Rhi::TextureStateEnum::ReadWrite),
          m_shading_normal_texture(name + "_gbuffer_normal_texture",
                                   device,
                                   Rhi::TextureCreateInfo(resolution.x,
                                                          resolution.y,
                                                          Rhi::FormatEnum::R16G16B16A16_SNorm,
                                                          Rhi::TextureUsageEnum::StorageImage |
                                                              Rhi::TextureUsageEnum::ColorAttachment),
                                   Rhi::TextureStateEnum::ReadWrite),
          m_diffuse_reflectance_texture(name + "_gbuffer_diffuse_reflectance_texture",
                                        device,
                                        Rhi::TextureCreateInfo(resolution.x,
                                                               resolution.y,
                                                               Rhi::FormatEnum::R11G11B10_UFloat,
                                                               Rhi::TextureUsageEnum::StorageImage |
                                                                   Rhi::TextureUsageEnum::ColorAttachment),
                                        Rhi::TextureStateEnum::ReadWrite),
          m_specular_reflectance_texture(name + "_gbuffer_specular_reflectance_texture",
                                         device,
                                         Rhi::TextureCreateInfo(resolution.x,
                                                                resolution.y,
                                                                Rhi::FormatEnum::R11G11B10_UFloat,
                                                                Rhi::TextureUsageEnum::StorageImage |
                                                                    Rhi::TextureUsageEnum::ColorAttachment),
                                         Rhi::TextureStateEnum::ReadWrite),
          m_specular_roughness_texture(name + "_gbuffer_roughness_texture",
                                       device,
                                       Rhi::TextureCreateInfo(resolution.x,
                                                              resolution.y,
                                                              Rhi::FormatEnum::R16_UNorm,
                                                              Rhi::TextureUsageEnum::StorageImage |
                                                                  Rhi::TextureUsageEnum::ColorAttachment),
                                       Rhi::TextureStateEnum::ReadWrite),
          m_diffuse_direct_result_texture(name + "_gbuffer_diffuse_direct_result_texture",
                                          device,
                                          Rhi::TextureCreateInfo(resolution.x,
                                                                 resolution.y,
                                                                 Rhi::FormatEnum::R11G11B10_UFloat,
                                                                 Rhi::TextureUsageEnum::StorageImage |
                                                                     Rhi::TextureUsageEnum::ColorAttachment),
                                          Rhi::TextureStateEnum::ReadWrite)
        {
        }
    };

    std::vector<Rhi::FramebufferBindings> m_raster_fbindings;
    std::vector<PerFlightRenderResource>  m_per_flight_resources;

    GuiEventCoordinator & m_gui_event_coordinator;
    GpuProfilerGui        m_gpu_profiler_gui;

    // Render passes
    GBufferGenerateRayTracePass m_pass_ray_trace_gbuffer_generate;
    PathTracingPass             m_pass_path_tracing;
    RenderToFramebufferPass     m_pass_render_to_framebuffer;

    Renderer(Rhi::Device &                               device,
             ShaderBinaryManager &                       shader_binary_manager,
             GuiEventCoordinator &                       gui_event_coordinator,
             const std::span<const Rhi::Texture * const> swapchain_attachment,
             const int2                                  resolution,
             const size_t                                num_flights)
    : m_raster_fbindings(ConstructFramebufferBinding(device, swapchain_attachment)),
      m_pass_ray_trace_gbuffer_generate(device, shader_binary_manager),
      m_pass_path_tracing(device, shader_binary_manager),
      m_pass_render_to_framebuffer(device, shader_binary_manager, m_raster_fbindings[0]),
      m_per_flight_resources(ConstructPerFlightResource(device, resolution, num_flights)),
      m_gui_event_coordinator(gui_event_coordinator),
      m_gpu_profiler_gui(num_flights)
    {
    }

    static std::vector<Rhi::FramebufferBindings>
    ConstructFramebufferBinding(const Rhi::Device &                           device,
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
    ConstructPerFlightResource(Rhi::Device & device, const int2 resolution, const size_t num_flights)
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
        m_raster_fbindings     = ConstructFramebufferBinding(device, swapchain_attachments);
        m_per_flight_resources = ConstructPerFlightResource(device, resolution, num_flights);
        m_pass_render_to_framebuffer.init_or_reload(device, shader_binary_manager, m_raster_fbindings[0]);
    }

    void
    loop(const RenderContext & ctx)
    {
        // Display the gui for params and human readable data
        m_gpu_profiler_gui.draw_gui(m_gui_event_coordinator);

        PerFlightRenderResource & per_flight_render_resource = m_per_flight_resources[ctx.m_flight_index];

        GpuProfiler * gpu_profiler = nullptr;
        if (!m_gpu_profiler_gui.m_pause)
        {
            // Summarize the information recorded in the gpu profiler
            gpu_profiler = &per_flight_render_resource.m_gpu_profiler;
            gpu_profiler->summarize();

            // Show the result of gpu profiler in a human readable format
            m_gpu_profiler_gui.update(gpu_profiler->m_profiling_intervals, gpu_profiler->m_ns_from_timestamp);

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
                GpuProfilingScope gbuffer_scope("Generate Gbuffer (Via Ray tracing)", cmd_buffer, gpu_profiler);
                m_pass_ray_trace_gbuffer_generate.render(cmd_buffer,
                                                         ctx,
                                                         per_flight_render_resource.m_depth_texture,
                                                         per_flight_render_resource.m_shading_normal_texture,
                                                         per_flight_render_resource.m_diffuse_reflectance_texture,
                                                         per_flight_render_resource.m_specular_reflectance_texture,
                                                         per_flight_render_resource.m_specular_roughness_texture,
                                                         ctx.m_resolution);
            }

            // Direct Light & GI Pass
            {
                GpuProfilingScope direct_light_scope("Path Tracing", cmd_buffer, gpu_profiler);
                m_pass_path_tracing.render(cmd_buffer,
                                           ctx,
                                           per_flight_render_resource.m_diffuse_direct_result_texture,
                                           per_flight_render_resource.m_depth_texture,
                                           per_flight_render_resource.m_shading_normal_texture,
                                           per_flight_render_resource.m_specular_roughness_texture,
                                           ctx.m_resolution);
            }

            // Transition
            {
                GpuProfilingScope transit_scope("Transit present to color attachment", cmd_buffer, gpu_profiler);
                cmd_buffer.transition_texture(ctx.m_per_swap_resource.m_swapchain_texture,
                                              Rhi::TextureStateEnum::Present,
                                              Rhi::TextureStateEnum::ColorAttachment);
            }

            // Run final pass
            {
                GpuProfilingScope render_to_framebuffer_scope("Render rtresult to framebuffer", cmd_buffer, gpu_profiler);
                m_pass_render_to_framebuffer.run(cmd_buffer,
                                                 ctx,
                                                 per_flight_render_resource.m_diffuse_direct_result_texture,
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
            }
        }

        // End recording
        cmd_buffer.end();
        cmd_buffer.submit(&ctx.m_per_flight_resource.m_flight_fence,
                          &ctx.m_per_flight_resource.m_image_ready_semaphore,
                          &ctx.m_per_flight_resource.m_image_presentable_semaphore);
    }
};