#pragma once

#include "render/shader_path.h"
#include "rhi/rhi.h"
#include "scene_resource.h"

struct BeautyPass
{
    Rhi::RasterPipeline m_raster_pipeline;
    Rhi::Sampler        m_sampler;

    BeautyPass() {}

    BeautyPass(const Rhi::Device & device, const ShaderManager & shader_manager, const Rhi::FramebufferBindings & fb)
    {
        m_sampler = Rhi::Sampler(&device);
        init_or_reload(device, shader_manager, fb);
    }

    void
    init_or_reload(const Rhi::Device & device, const ShaderManager & shader_manager, const Rhi::FramebufferBindings & fb)
    {
        std::array<Rhi::ShaderSrc, 2> srcs;
        srcs[0] =
            Rhi::ShaderSrc(Rhi::ShaderStageEnum::Vertex, BASE_SHADER_DIR "beauty.hlsl", "VsMain");
        srcs[1] =
            Rhi::ShaderSrc(Rhi::ShaderStageEnum::Fragment, BASE_SHADER_DIR "beauty.hlsl", "FsMain");
        m_raster_pipeline = Rhi::RasterPipeline(device, srcs, shader_manager, fb, "final_composite_pipeline");
    }

    void
    run(Rhi::CommandList & cmd_list, const RenderContext & ctx, const Rhi::Texture & tex, const Rhi::FramebufferBindings & fb)
    {
        // begin render pass
        cmd_list.begin_render_pass(fb);

        // draw result
        cmd_list.bind_raster_pipeline(m_raster_pipeline);

        // setup descriptor set
        std::array<Rhi::DescriptorSet, 1> beauty_desc_sets;
        beauty_desc_sets[0] =
            Rhi::DescriptorSet(&ctx.m_device, m_raster_pipeline, &ctx.m_descriptor_pool, 0);
        beauty_desc_sets[0].set_t_texture(0, tex).set_s_sampler(0, m_sampler).update();

        // raster
        cmd_list.bind_graphics_descriptor_set(beauty_desc_sets);
        cmd_list.bind_vertex_buffer(ctx.m_scene_resource.m_d_vbuf_position, sizeof(CompactVertex));
        cmd_list.bind_index_buffer(ctx.m_scene_resource.m_d_ibuf, Rhi::IndexType::Uint32);
        cmd_list.draw_instanced(3, 1, 0, 0);

        // end render pass
        cmd_list.end_render_pass();
    }
};
