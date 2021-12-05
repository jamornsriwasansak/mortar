#pragma once

#include "core/vmath.h"
#include "passes/final_composite.h"
#include "passes/ray_trace_path_tracer.h"
#include "passes/ray_trace_visualize.h"
#include "render_context.h"
#include "rhi/rhi.h"
#include "scene_resource.h"

struct Renderer
{
    struct PerFlightResource
    {
        Rhi::Texture m_rt_result;
    };

    std::vector<Rhi::FramebufferBindings> m_raster_fbindings;
    std::vector<PerFlightResource>        m_per_flight_resources;

    // all raytrace mode (rays start from the camera)
    enum class RayTraceMode : int
    {
        VisualizeDebug,
        PrimitivePathTracing
    };
    RayTraceMode            m_ray_trace_mode = RayTraceMode::VisualizeDebug;
    RayTraceVisualizePass   m_pass_ray_trace_visualize;
    RayTracePathTracer      m_pass_ray_trace_primitive_pathtrace;
    RenderToFramebufferPass m_pass_render_to_framebuffer;

    Renderer(const Rhi::Device &                         device,
             const ShaderBinaryManager &                 shader_binary_manager,
             const std::span<const Rhi::Texture * const> swapchain_attachment,
             const int2                                  resolution,
             const size_t                                num_flights)
    : m_raster_fbindings(construct_framebuffer_bindings(device, swapchain_attachment)),
      m_pass_ray_trace_visualize(device, shader_binary_manager),
      m_pass_ray_trace_primitive_pathtrace(device, shader_binary_manager),
      m_pass_render_to_framebuffer(device, shader_binary_manager, m_raster_fbindings[0]),
      m_per_flight_resources(construct_per_flight_resource(device, resolution, num_flights))
    {
    }

    std::vector<Rhi::FramebufferBindings>
    construct_framebuffer_bindings(const Rhi::Device &                           device,
                                   const std::span<const Rhi::Texture * const> & swapchain_attachment)
    {
        std::vector<Rhi::FramebufferBindings> result;
        result.reserve(swapchain_attachment.size());
        for (size_t i = 0; i < swapchain_attachment.size(); i++)
        {
            const std::array<const Rhi::Texture *, 1> attachments = { swapchain_attachment[i] };
            result.emplace_back("framebuffer_bindings", device, attachments);
        }
        return result;
    }

    std::vector<PerFlightResource>
    construct_per_flight_resource(const Rhi::Device & device, const int2 resolution, const size_t num_flights)
    {
        std::vector<PerFlightResource> result;
        result.reserve(num_flights);
        for (size_t i = 0; i < num_flights; i++)
        {
            result.emplace_back(Rhi::Texture(&device,
                                             Rhi::TextureUsageEnum::StorageImage | Rhi::TextureUsageEnum::ColorAttachment |
                                                 Rhi::TextureUsageEnum::Sampled,
                                             Rhi::TextureStateEnum::NonFragmentShaderVisible,
                                             Rhi::FormatEnum::R32G32B32A32_SFloat,
                                             resolution,
                                             nullptr,
                                             nullptr,
                                             float4(0.0f, 0.0f, 0.0f, 0.0f),
                                             "flight_" + std::to_string(i) + "_ray_tracing_result"));
        }
        return result;
    }

    void
    resize(const Rhi::Device &                           device,
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
        Rhi::CommandList          cmd_list = ctx.m_graphics_command_pool.get_command_list();
        const PerFlightResource & per_flight_resource = m_per_flight_resources[ctx.m_flight_index];

        if (ctx.m_is_shaders_dirty)
        {
            // init_or_reload_shader(ctx);
        }

        cmd_list.begin();

        // raytrace mode
        bool p_open = true;
        m_pass_ray_trace_visualize.draw_gui();
        {
            if (ImGui::Begin("Renderer Raytrace Mode", &p_open))
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
                m_pass_ray_trace_visualize.render(cmd_list, ctx, per_flight_resource.m_rt_result, ctx.m_resolution);
                break;
            case Renderer::RayTraceMode::PrimitivePathTracing:
                m_pass_ray_trace_primitive_pathtrace.render(cmd_list, ctx, per_flight_resource.m_rt_result, ctx.m_resolution);
                break;
            default:
                break;
            }
        }

        // transition
        cmd_list.transition_texture(per_flight_resource.m_rt_result,
                                    Rhi::TextureStateEnum::NonFragmentShaderVisible,
                                    Rhi::TextureStateEnum::FragmentShaderVisible);
        cmd_list.transition_texture(ctx.m_swapchain_texture,
                                    Rhi::TextureStateEnum::Present,
                                    Rhi::TextureStateEnum::ColorAttachment);

        // run final pass
        m_pass_render_to_framebuffer.run(cmd_list,
                                         ctx,
                                         per_flight_resource.m_rt_result,
                                         m_raster_fbindings[ctx.m_image_index]);

        // render imgui onto swapchain
        if (ctx.m_should_imgui_drawn)
        {
            cmd_list.render_imgui(ctx.m_imgui_render_pass, ctx.m_image_index);
        }

        // transition
        cmd_list.transition_texture(ctx.m_swapchain_texture,
                                    Rhi::TextureStateEnum::ColorAttachment,
                                    Rhi::TextureStateEnum::Present);
        cmd_list.transition_texture(per_flight_resource.m_rt_result,
                                    Rhi::TextureStateEnum::FragmentShaderVisible,
                                    Rhi::TextureStateEnum::NonFragmentShaderVisible);

        cmd_list.end();
        cmd_list.submit(&ctx.m_flight_fence, &ctx.m_image_ready_semaphore, &ctx.m_image_presentable_semaphore);
    }
};