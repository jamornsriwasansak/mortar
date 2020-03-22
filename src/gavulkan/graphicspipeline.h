#pragma once

#include <vulkan/vulkan.hpp>

#include "gavulkan/attachment.h"
#include "gavulkan/shader.h"
#include "gavulkan/descriptor.h"

struct GraphicsPipeline
{
	vk::UniqueRenderPass					m_vk_render_pass;
	vk::UniquePipelineLayout				m_vk_pipeline_layout;
	vk::UniquePipeline						m_vk_pipeline;
	std::vector<vk::UniqueFramebuffer>		m_vk_framebuffers;
	DescriptorManager						m_descriptor_manager;

	GraphicsPipeline(const std::vector<const Shader *> shaders,
					 const std::vector<RgbaColorAttachment> & color_attachments,
					 const std::optional<DepthAttachment> & depth_attachment = std::nullopt):
		m_descriptor_manager(shaders)
	{
		m_vk_render_pass = create_vk_render_pass(color_attachments,
												 depth_attachment);
		auto pipeline_result = create_vk_pipeline(*m_vk_render_pass,
												  shaders,
												  m_descriptor_manager.get_vk_descriptor_set_layouts(),
												  m_descriptor_manager.m_vk_push_constant_range,
												  color_attachments,
												  depth_attachment.has_value());
		m_vk_pipeline_layout = std::move(pipeline_result.m_pipeline_layout);
		m_vk_pipeline = std::move(pipeline_result.m_pipeline);
		m_vk_framebuffers = create_vk_framebuffers(color_attachments,
												   depth_attachment,
												   *m_vk_render_pass);
	}

	DescriptorSetsBuilder
	build_descriptor_sets(const size_t i_set) const
	{
		return m_descriptor_manager.make_descriptor_sets_builder(i_set);
	}

	vk::RenderPassBeginInfo
	get_render_pass_begin_info(const size_t i_frame) const
	{
		static std::array<float, 4> clear_color_value = {0.0f, 0.0f, 0.0f, 1.0f};
		static vk::ClearDepthStencilValue clear_depth_value = { 1.0f, 0 };
		static vk::ClearValue clear_color = vk::ClearValue().setColor(clear_color_value);
		static vk::ClearValue clear_depth = vk::ClearValue().setDepthStencil(clear_depth_value);
		static std::array<vk::ClearValue, 2> clear_values = { clear_color, clear_depth };
		vk::RenderPassBeginInfo render_pass_begin = {};
		{
			render_pass_begin.setRenderPass(*m_vk_render_pass);
			render_pass_begin.setFramebuffer(*m_vk_framebuffers[i_frame]);
			render_pass_begin.renderArea.setExtent(vk::Extent2D(Extent.width, Extent.height));
			render_pass_begin.renderArea.setOffset({ 0, 0 });
			render_pass_begin.setClearValueCount(2);
			render_pass_begin.setPClearValues(clear_values.data());
		}
		return render_pass_begin;
	}

private:

	vk::UniqueRenderPass
	create_vk_render_pass(const std::vector<RgbaColorAttachment> & color_attachments,
						  const std::optional<DepthAttachment> & depth_attachment)
	{
		// color attachment
		std::vector<vk::AttachmentDescription> color_attachment_descriptions;
		std::vector<vk::AttachmentReference> color_attachment_refs;
		for (size_t i_color_attachment = 0; i_color_attachment < color_attachments.size(); i_color_attachment++)
		{
			// pushback
			const RgbaColorAttachment & color_attachment = color_attachments[i_color_attachment];
			color_attachment_descriptions.push_back(color_attachment.m_vk_attachment_description);

			// setup color attachment reference
			vk::AttachmentReference color_attch_ref = {};
			{
				color_attch_ref.setAttachment(static_cast<uint32_t>(i_color_attachment));
				color_attch_ref.setLayout(vk::ImageLayout::eColorAttachmentOptimal);
			}
			color_attachment_refs.push_back(color_attch_ref);
		}

		// for simplicity, we plan to use only single subpass (for now)
		vk::SubpassDescription subpass = {};
		{
			subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);

			// color attachment
			if (color_attachments.size() > 0)
			{
				subpass.setColorAttachmentCount(static_cast<uint32_t>(color_attachment_refs.size()));
				subpass.setPColorAttachments(color_attachment_refs.data());
			}

			// depth attachment
			if (depth_attachment)
			{
				subpass.setPDepthStencilAttachment(&depth_attachment->m_vk_depth_attachment_ref);
			}
		}

		vk::SubpassDependency dependency = {};
		{
			dependency.setSrcSubpass(VK_SUBPASS_EXTERNAL);
			dependency.setDstSubpass(0);
			dependency.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
			dependency.setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
			dependency.setSrcAccessMask(vk::AccessFlags());
			dependency.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
		}

		// create array of all attachments (color + depth)
		std::vector<vk::AttachmentDescription> attachment_descriptions;
		{
			attachment_descriptions.insert(attachment_descriptions.end(),
										   color_attachment_descriptions.begin(),
										   color_attachment_descriptions.end());
			if (depth_attachment)
			{
				attachment_descriptions.push_back(depth_attachment->m_vk_attachment_description);
			}
		}

		vk::RenderPassCreateInfo render_pass_ci = {};
		{
			render_pass_ci.setAttachmentCount(static_cast<uint32_t>(attachment_descriptions.size()));
			render_pass_ci.setPAttachments(attachment_descriptions.data());
			render_pass_ci.setSubpassCount(1);
			render_pass_ci.setPSubpasses(&subpass);
			render_pass_ci.setDependencyCount(1);
			render_pass_ci.setPDependencies(&dependency);
		}

		return Core::Inst().m_vk_device->createRenderPassUnique(render_pass_ci);
	}

	struct PipelineReturnType
	{
		vk::UniquePipelineLayout				m_pipeline_layout;
		vk::UniquePipeline						m_pipeline;
	};
	PipelineReturnType
	create_vk_pipeline(const vk::RenderPass & render_pass,
					   const std::vector<const Shader *> & shaders,
					   const std::vector<vk::DescriptorSetLayout> & vk_descriptor_set_layouts,
					   const std::optional<vk::PushConstantRange> & vk_push_constant_range,
					   const std::vector<RgbaColorAttachment> & color_attachments,
					   const bool has_depth_attachment)
	{
		// shader stages
		std::vector<vk::PipelineShaderStageCreateInfo> shader_stage_cis;
		for (size_t i_shader = 0; i_shader < shaders.size(); i_shader++)
		{
			shader_stage_cis.push_back(shaders[i_shader]->get_pipeline_shader_stage_ci());
		}

		// find vs shader stage
		const Shader * vs_shader = nullptr;
		for (size_t i_shader = 0; i_shader < shaders.size(); i_shader++)
		{
			if (shaders[i_shader]->m_vk_shader_stage == vk::ShaderStageFlagBits::eVertex)
			{
				vs_shader = shaders[i_shader];
			}
		}

		// vertex inputs and their formats
		std::vector<vk::VertexInputBindingDescription> vertex_input_bindings(vs_shader->m_attributes.size());
		std::vector<vk::VertexInputAttributeDescription> vertex_input_attributes(vs_shader->m_attributes.size());

		static const std::map<std::string, std::pair<size_t, vk::Format>> map_from_attribute_type =
		{
			{ "vec4", { sizeof(vec4), vk::Format::eR32G32B32A32Sfloat } },
			{ "vec3", { sizeof(vec3), vk::Format::eR32G32B32Sfloat } },
			{ "vec2", { sizeof(vec2), vk::Format::eR32G32Sfloat } },
			{ "float", { sizeof(float), vk::Format::eR32Sfloat } }
		};

		for (size_t i_attribute = 0; i_attribute < vs_shader->m_attributes.size(); i_attribute++)
		{
			const ShaderAttributeInfo & shader_attribute = vs_shader->m_attributes[i_attribute];
			const auto [stride, format] = map_from_attribute_type.at(shader_attribute.m_type);

			vertex_input_bindings[i_attribute].setBinding(shader_attribute.m_location);
			vertex_input_bindings[i_attribute].setStride(static_cast<uint32_t>(stride));
			vertex_input_bindings[i_attribute].setInputRate(vk::VertexInputRate::eVertex);

			vertex_input_attributes[i_attribute].setBinding(shader_attribute.m_location);
			vertex_input_attributes[i_attribute].setLocation(shader_attribute.m_location);
			vertex_input_attributes[i_attribute].setFormat(format);
			vertex_input_attributes[i_attribute].setOffset(0);
		}

		vk::PipelineVertexInputStateCreateInfo vertex_input_state_ci;
		{
			vertex_input_state_ci.setVertexBindingDescriptionCount(static_cast<uint32_t>(vertex_input_bindings.size()));
			vertex_input_state_ci.setPVertexBindingDescriptions(vertex_input_bindings.data());
			vertex_input_state_ci.setVertexAttributeDescriptionCount(static_cast<uint32_t>(vertex_input_attributes.size()));
			vertex_input_state_ci.setPVertexAttributeDescriptions(vertex_input_attributes.data());
		}

		vk::PipelineInputAssemblyStateCreateInfo input_assembly_state_ci;
		{
			input_assembly_state_ci.setTopology(vk::PrimitiveTopology::eTriangleList);
			input_assembly_state_ci.setPrimitiveRestartEnable(VK_FALSE);
		}

		vk::PipelineLayoutCreateInfo layout_ci = {};
		{
			layout_ci.setSetLayoutCount(static_cast<uint32_t>(vk_descriptor_set_layouts.size()));
			layout_ci.setPSetLayouts(vk_descriptor_set_layouts.data());
			if (vk_push_constant_range.has_value())
			{
				layout_ci.setPPushConstantRanges(&vk_push_constant_range.value());
				layout_ci.setPushConstantRangeCount(1);
			}
		}
		vk::UniquePipelineLayout layout = Core::Inst().m_vk_device->createPipelineLayoutUnique(layout_ci);

		// viewport
		vk::Viewport viewport = {};
		{
			viewport.setX(0.0f);
			viewport.setY(0.0f);
			viewport.setWidth(static_cast<float>(Extent.width));
			viewport.setHeight(static_cast<float>(Extent.height));
			viewport.setMinDepth(0.0f);
			viewport.setMaxDepth(1.0f);
		}

		vk::Rect2D scissor = {};
		{
			scissor.setOffset(vk::Offset2D(0, 0));
			scissor.setExtent(Extent);
		}

		vk::PipelineViewportStateCreateInfo viewport_state_ci = {};
		{
			viewport_state_ci.setViewportCount(1);
			viewport_state_ci.setPViewports(&viewport);
			viewport_state_ci.setScissorCount(1);
			viewport_state_ci.setPScissors(&scissor);
		}

		// rasterization
		vk::PipelineRasterizationStateCreateInfo rasterization_state_ci = {};
		{
			rasterization_state_ci.setDepthClampEnable(VK_FALSE);
			rasterization_state_ci.setRasterizerDiscardEnable(VK_FALSE);
			rasterization_state_ci.setPolygonMode(vk::PolygonMode::eFill);
			rasterization_state_ci.setLineWidth(1.0f);
			rasterization_state_ci.setCullMode(vk::CullModeFlagBits::eBack);
			rasterization_state_ci.setFrontFace(vk::FrontFace::eCounterClockwise);
			rasterization_state_ci.setDepthBiasEnable(VK_FALSE);
			rasterization_state_ci.setDepthBiasConstantFactor(0.0f);
			rasterization_state_ci.setDepthBiasClamp(0.0f);
			rasterization_state_ci.setDepthBiasSlopeFactor(0.0f);
		}

		// multisampling
		vk::PipelineMultisampleStateCreateInfo multisampling_state_ci = {};
		{
			multisampling_state_ci.setSampleShadingEnable(VK_FALSE);
			multisampling_state_ci.setRasterizationSamples(vk::SampleCountFlagBits::e1);
			multisampling_state_ci.setMinSampleShading(1.0f);
			multisampling_state_ci.setPSampleMask(nullptr);
			multisampling_state_ci.setAlphaToCoverageEnable(VK_FALSE);
			multisampling_state_ci.setAlphaToOneEnable(VK_FALSE);
		}

		// depth stencil state
		vk::PipelineDepthStencilStateCreateInfo depth_stencil_state_ci = {};
		{
			depth_stencil_state_ci.setDepthTestEnable(true);
			depth_stencil_state_ci.setDepthWriteEnable(true);
			depth_stencil_state_ci.setDepthCompareOp(vk::CompareOp::eLess);
			depth_stencil_state_ci.setDepthBoundsTestEnable(false);
			depth_stencil_state_ci.setMinDepthBounds(0.0f);
			depth_stencil_state_ci.setMaxDepthBounds(1.0f);
			depth_stencil_state_ci.setStencilTestEnable(false);
		}

		// color blending
		std::vector<vk::PipelineColorBlendAttachmentState> color_blend_attachment_states(color_attachments.size());
		for (size_t i_attachment = 0; i_attachment < color_attachments.size(); i_attachment++)
		{
			color_blend_attachment_states[i_attachment] = color_attachments[i_attachment].m_vk_color_blend_attachment_state;
		}
		vk::PipelineColorBlendStateCreateInfo color_blend_state_ci = {};
		{
			color_blend_state_ci.setLogicOpEnable(VK_FALSE);
			color_blend_state_ci.setLogicOp(vk::LogicOp::eCopy);
			color_blend_state_ci.setAttachmentCount(static_cast<uint32_t>(color_blend_attachment_states.size()));
			color_blend_state_ci.setPAttachments(color_blend_attachment_states.data());
			color_blend_state_ci.setBlendConstants({ 0.0f, 0.0f, 0.0f, 0.0f });
		}

		// dynamic state
		vk::PipelineDynamicStateCreateInfo dynamic_state_ci = {};
		dynamic_state_ci.setDynamicStateCount(0);

		// pipeline create info
		vk::GraphicsPipelineCreateInfo pipeline_ci = {};
		{
			pipeline_ci.setStageCount(static_cast<uint32_t>(shader_stage_cis.size()));
			pipeline_ci.setPStages(shader_stage_cis.data());
			pipeline_ci.setPVertexInputState(&vertex_input_state_ci);
			pipeline_ci.setPInputAssemblyState(&input_assembly_state_ci);
			pipeline_ci.setPMultisampleState(&multisampling_state_ci);
			if (has_depth_attachment)
			{
				pipeline_ci.setPDepthStencilState(&depth_stencil_state_ci);
			}
			else
			{
				pipeline_ci.setPDepthStencilState(nullptr);
			}
			pipeline_ci.setPColorBlendState(&color_blend_state_ci);
			pipeline_ci.setPDynamicState(nullptr);
			pipeline_ci.setLayout(*layout);
			pipeline_ci.setRenderPass(render_pass);
			pipeline_ci.setSubpass(0);
			pipeline_ci.setBasePipelineHandle(nullptr);
			pipeline_ci.setBasePipelineIndex(-1);
			pipeline_ci.setPRasterizationState(&rasterization_state_ci);
			pipeline_ci.setPViewportState(&viewport_state_ci);
		}

		// create pipeline
		vk::UniquePipeline pipeline = Core::Inst().m_vk_device->createGraphicsPipelineUnique(nullptr,
																							 pipeline_ci);

		return { std::move(layout),
				 std::move(pipeline) };
	}

	std::vector<vk::UniqueFramebuffer>
	create_vk_framebuffers(const std::vector<RgbaColorAttachment> & color_attachments,
						   const std::optional<DepthAttachment> & depth_attachment,
						   const vk::RenderPass & render_pass)
	{
		std::vector<vk::UniqueFramebuffer> framebuffers;

		// for every framebuffer in the swapchain
		for (size_t i_frame = 0; i_frame < color_attachments[0].m_vk_image_views.size(); i_frame++)
		{
			// color attachment
			std::vector<vk::ImageView> attachments;
			for (size_t i_attachment = 0; i_attachment < color_attachments.size(); i_attachment++)
			{
				attachments.push_back(*color_attachments[i_attachment].m_vk_image_views[i_frame]);
			}

			// depth attachment
			if (depth_attachment)
			{
				// if we have depth_image_view, we have to include it as a part of attachments in every frame
				attachments.push_back(*depth_attachment->m_vk_image_view);
			}

			vk::FramebufferCreateInfo framebuffer_ci = {};
			{
				framebuffer_ci.setRenderPass(render_pass);
				framebuffer_ci.setAttachmentCount(static_cast<uint32_t>(attachments.size()));
				framebuffer_ci.setPAttachments(attachments.data());
				framebuffer_ci.setWidth(Extent.width);
				framebuffer_ci.setHeight(Extent.height);
				framebuffer_ci.setLayers(1);
			}

			vk::UniqueFramebuffer framebuffer = Core::Inst().m_vk_device->createFramebufferUnique(framebuffer_ci);
			framebuffers.push_back(std::move(framebuffer));
		}
		return framebuffers;
	}
};