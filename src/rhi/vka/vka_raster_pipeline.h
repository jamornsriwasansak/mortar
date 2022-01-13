#pragma once

#include "rhi/rhi.h"

#ifdef USE_VKA

    #include "rhi/shadercompiler/shader_binary_manager.h"
    #include "spirv_reflection.h"
    #include "vka_common.h"
    #include "vka_device.h"
    #include "vka_framebuffer_binding.h"

namespace VKA_NAME
{
struct RasterPipelineState
{
    int2                                     m_resolution;
    vk::Viewport                             m_viewport;
    vk::Rect2D                               m_scissor;
    vk::PipelineRasterizationStateCreateInfo m_rasterizer;
    vk::PipelineMultisampleStateCreateInfo   m_multi_sample;
    vk::PipelineInputAssemblyStateCreateInfo m_input_assembly;
    vk::PipelineColorBlendStateCreateInfo    m_color_blend;
    std::vector<vk::DynamicState>            m_dynamic_states;

    RasterPipelineState(const int2 & resolution) : m_resolution(resolution)
    {
        constexpr vk::SampleCountFlagBits num_msaa_samples = vk::SampleCountFlagBits::e1;

        vk::Viewport();
        {
            m_viewport.setX(0.0f);
            m_viewport.setY(static_cast<float>(m_resolution.y));
            m_viewport.setWidth(static_cast<float>(m_resolution.x));
            m_viewport.setHeight(-static_cast<float>(m_resolution.y));
            m_viewport.setMinDepth(0.0f);
            m_viewport.setMaxDepth(1.0f);
        }

        m_scissor = vk::Rect2D();
        {
            m_scissor.setExtent(vk::Extent2D(m_resolution.x, m_resolution.y));
            m_scissor.setOffset(vk::Offset2D(0, 0));
        }

        m_rasterizer = vk::PipelineRasterizationStateCreateInfo();
        {
            m_rasterizer.setDepthClampEnable(VK_FALSE);
            m_rasterizer.setRasterizerDiscardEnable(VK_FALSE);
            m_rasterizer.setPolygonMode(vk::PolygonMode::eFill);
            m_rasterizer.setLineWidth(1.0f);
            m_rasterizer.setCullMode(vk::CullModeFlagBits::eBack);
            m_rasterizer.setFrontFace(vk::FrontFace::eCounterClockwise);
            m_rasterizer.setDepthBiasEnable(VK_FALSE);
            m_rasterizer.setDepthBiasConstantFactor(0.0f);
            m_rasterizer.setDepthBiasClamp(0.0f);
            m_rasterizer.setDepthBiasSlopeFactor(0.0f);
        }

        m_multi_sample = vk::PipelineMultisampleStateCreateInfo();
        {
            constexpr bool use_msaa = num_msaa_samples != vk::SampleCountFlagBits::e1;
            m_multi_sample.setSampleShadingEnable(use_msaa);
            m_multi_sample.setRasterizationSamples(num_msaa_samples);
            m_multi_sample.setMinSampleShading(1.0f);
            m_multi_sample.setPSampleMask(nullptr);
            m_multi_sample.setAlphaToCoverageEnable(VK_FALSE);
            m_multi_sample.setAlphaToOneEnable(VK_FALSE);
        }

        m_input_assembly = vk::PipelineInputAssemblyStateCreateInfo();
        {
            m_input_assembly.setTopology(vk::PrimitiveTopology::eTriangleList);
            m_input_assembly.setPrimitiveRestartEnable(VK_FALSE);
        }

        m_color_blend = vk::PipelineColorBlendStateCreateInfo();
        {
            m_color_blend.setLogicOpEnable(VK_FALSE);
            m_color_blend.setLogicOp(vk::LogicOp::eCopy);
            m_color_blend.setBlendConstants({ 0.0f, 0.0f, 0.0f, 0.0f });
            // attachments will set later automatically
        }
    }
};

struct RasterPipeline
{
    vk::UniquePipeline                         m_vk_pipeline;
    vk::UniquePipelineLayout                   m_vk_pipeline_layout;
    std::vector<vk::UniqueDescriptorSetLayout> m_vk_descriptor_set_layouts;

    RasterPipeline(const std::string &                name,
                   const Device &                     device,
                   const std::span<const ShaderSrc> & shader_srcs,
                   const ShaderBinaryManager &        shader_binary_manager,
                   const FramebufferBindings &        framebuffer_bindings)
    {
        // compile all shader srcs
        std::vector<std::vector<std::byte>> spirv_codes(shader_srcs.size());
        for (size_t i = 0; i < shader_srcs.size(); i++)
        {
            spirv_codes[i] = shader_binary_manager.get_cached_shader(shader_srcs[i]);
        }

        // reflection
        SpirvReflector     spirv_reflector;
        VkReflectionResult reflection = spirv_reflector.reflect(shader_srcs, spirv_codes);

        // vertex input state
        std::vector<vk::VertexInputBindingDescription> vk_input_bindings(
            reflection.m_vertex_input_attribs.size());

        // setup input bindings
        vk::PipelineVertexInputStateCreateInfo vs_input_info = {};
        vs_input_info.setVertexBindingDescriptions(vk_input_bindings);
        vs_input_info.setVertexAttributeDescriptions(reflection.m_vertex_input_attribs);

        // attachment state
        vk::PipelineColorBlendAttachmentState color_blend_Attachment{};
        color_blend_Attachment.setColorWriteMask(
            vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
        color_blend_Attachment.setBlendEnable(VK_FALSE);

        // create shader modules
        std::vector<vk::UniqueShaderModule> shader_modules(reflection.m_shader_stage_flags.size());
        for (size_t i = 0; i < reflection.m_shader_stage_flags.size(); i++)
        {
            vk::ShaderModuleCreateInfo shader_module_ci;
            shader_module_ci.setPCode(reinterpret_cast<uint32_t *>(spirv_codes[i].data()));
            shader_module_ci.setCodeSize(spirv_codes[i].size());
            shader_modules[i] = device.m_vk_ldevice->createShaderModuleUnique(shader_module_ci);
        }

        // attach shader stages
        std::vector<vk::PipelineShaderStageCreateInfo> shader_stages(reflection.m_shader_stage_flags.size());
        for (size_t i = 0; i < reflection.m_shader_stage_flags.size(); i++)
        {
            vk::PipelineShaderStageCreateInfo & shader_stage_ci = shader_stages[i];
            shader_stage_ci.setStage(reflection.m_shader_stage_flags[i]);
            shader_stage_ci.setModule(*shader_modules[i]);
            shader_stage_ci.setPName(shader_srcs[i].m_entry.c_str());
        }

        // per pipeline state
        RasterPipelineState                 per_pipeline_state(framebuffer_bindings.m_resolution);
        vk::PipelineViewportStateCreateInfo viewport_state{};
        viewport_state.setViewportCount(1);
        viewport_state.setPViewports(&per_pipeline_state.m_viewport);
        viewport_state.setScissorCount(1);
        viewport_state.setPScissors(&per_pipeline_state.m_scissor);

        vk::PipelineColorBlendStateCreateInfo color_blending_ci = per_pipeline_state.m_color_blend;
        color_blending_ci.setPAttachments(&color_blend_Attachment);
        color_blending_ci.setAttachmentCount(1);

        vk::PipelineDynamicStateCreateInfo dynamic_state_ci = {};
        dynamic_state_ci.setDynamicStates(per_pipeline_state.m_dynamic_states);

        // descriptor layout
        Logger::Info(__FUNCTION__ " creating pipeline layout");
        for (auto & bindings : reflection.m_descriptor_set_bindings)
        {
            vk::DescriptorSetLayoutBindingFlagsCreateInfo binding_flag_ci;
            vk::DescriptorBindingFlags flags = vk::DescriptorBindingFlagBits::ePartiallyBound;
            std::vector<vk::DescriptorBindingFlags> binding_flags(bindings.size(), flags);
            binding_flag_ci.setBindingFlags(binding_flags);

            vk::DescriptorSetLayoutCreateInfo desc_layout_info_ci;
            desc_layout_info_ci.setBindings(bindings);
            desc_layout_info_ci.setPNext(&binding_flag_ci);
            m_vk_descriptor_set_layouts.emplace_back(
                device.m_vk_ldevice->createDescriptorSetLayoutUnique(desc_layout_info_ci));
        }

        std::vector<vk::DescriptorSetLayout> descriptor_layout = vk::uniqueToRaw(m_vk_descriptor_set_layouts);
        vk::PipelineLayoutCreateInfo pipeline_layout_ci = {};
        pipeline_layout_ci.setSetLayoutCount(static_cast<uint32_t>(descriptor_layout.size()));
        pipeline_layout_ci.setPSetLayouts(descriptor_layout.data());
        pipeline_layout_ci.setPushConstantRangeCount(0);
        pipeline_layout_ci.setPPushConstantRanges(nullptr);
        m_vk_pipeline_layout = device.m_vk_ldevice->createPipelineLayoutUnique(pipeline_layout_ci);

        // create pipeline info
        vk::GraphicsPipelineCreateInfo pipelineInfo;
        pipelineInfo.setStages(shader_stages);
        pipelineInfo.setPVertexInputState(&vs_input_info);
        pipelineInfo.setPInputAssemblyState(&per_pipeline_state.m_input_assembly);
        pipelineInfo.setPRasterizationState(&per_pipeline_state.m_rasterizer);
        pipelineInfo.setPMultisampleState(&per_pipeline_state.m_multi_sample);
        pipelineInfo.setPDepthStencilState(nullptr);
        pipelineInfo.setPColorBlendState(&color_blending_ci);
        pipelineInfo.setPDynamicState(&dynamic_state_ci);
        pipelineInfo.setLayout(m_vk_pipeline_layout.get());
        pipelineInfo.setRenderPass(framebuffer_bindings.m_vk_render_pass.get());
        pipelineInfo.setPViewportState(&viewport_state);
        pipelineInfo.setSubpass(0);
        pipelineInfo.setBasePipelineHandle(nullptr);
        pipelineInfo.setBasePipelineIndex(-1);

        Logger::Info(__FUNCTION__ " creating graphics pipeline");
        auto result = device.m_vk_ldevice->createGraphicsPipelineUnique(nullptr, pipelineInfo);
        VKCK(result.result);
        m_vk_pipeline = std::move(result.value);

        device.name_vkhpp_object<vk::Pipeline, vk::Pipeline::CType>(m_vk_pipeline.get(), name);
    }

    void
    resize()
    {
    }
};
} // namespace VKA_NAME
#endif