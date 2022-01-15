#pragma once

#include "application/render/shader_path.h"
#include "application/scene_resource.h"
#include "rhi/rhi.h"

struct RenderToFramebufferPass
{
    Rhi::RasterPipeline m_raster_pipeline;
    Rhi::Sampler        m_sampler;

    RenderToFramebufferPass(const Rhi::Device &              device,
                            ShaderBinaryManager &            shader_binary_manager,
                            const Rhi::FramebufferBindings & fb)
    : m_sampler("render_to_framebuffer_sampler", device),
      m_raster_pipeline("final_composite_pipeline", device, get_shader_srcs(), shader_binary_manager, fb)
    {
        // init_or_reload(device, shader_binary_manager, fb);
    }

    std::array<Rhi::ShaderSrc, 2>
    get_shader_srcs()
    {
        std::array<Rhi::ShaderSrc, 2> srcs;
        srcs[0] =
            Rhi::ShaderSrc(Rhi::ShaderStageEnum::Vertex, BASE_SHADER_DIR "beauty.hlsl", "VsMain");
        srcs[1] =
            Rhi::ShaderSrc(Rhi::ShaderStageEnum::Fragment, BASE_SHADER_DIR "beauty.hlsl", "FsMain");
        return srcs;
    }

    void
    init_or_reload([[maybe_unused]] const Rhi::Device &              device,
                   [[maybe_unused]] ShaderBinaryManager &            shader_binary_manager,
                   [[maybe_unused]] const Rhi::FramebufferBindings & fb)
    {
    }

    void
    run(Rhi::CommandBuffer & cmd_buffer, const RenderContext & ctx, const Rhi::Texture & tex, const Rhi::FramebufferBindings & fb)
    {
        // begin render pass
        cmd_buffer.begin_render_pass(fb);

        // draw result
        cmd_buffer.bind_raster_pipeline(m_raster_pipeline);

        // setup descriptor set
        std::array<Rhi::DescriptorSet, 1> beauty_desc_sets = {
            Rhi::DescriptorSet(ctx.m_device, m_raster_pipeline, ctx.m_per_flight_resource.m_descriptor_pool, 0)
        };
        beauty_desc_sets[0].set_t_texture(0, tex).set_s_sampler(0, m_sampler).update();

        // raster
        cmd_buffer.bind_graphics_descriptor_set(beauty_desc_sets);
        cmd_buffer.bind_vertex_buffer(ctx.m_scene_resource.m_d_vbuf_position, sizeof(CompactVertex));
        cmd_buffer.bind_index_buffer(ctx.m_scene_resource.m_d_ibuf, Rhi::IndexType::Uint32);
        cmd_buffer.draw_instanced(3, 1, 0, 0);

        // end render pass
        cmd_buffer.end_render_pass();
    }
};
