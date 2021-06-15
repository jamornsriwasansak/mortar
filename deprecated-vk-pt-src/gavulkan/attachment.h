#pragma once

#include <vulkan/vulkan.hpp>
#include <gavulkan/image.h>

struct RgbaColorAttachment
{
	vk::AttachmentDescription				m_vk_attachment_description;
	vk::PipelineColorBlendAttachmentState	m_vk_color_blend_attachment_state;
	vk::ClearColorValue						m_vk_clear_color_value;
	std::vector<const vk::ImageView *>		m_vk_image_views;

	RgbaColorAttachment()
	{
	}

	void init()
	{
		// attachment description
		{
			// format
			m_vk_attachment_description.setFormat(vk::Format::eB8G8R8A8Unorm);

			// samples
			m_vk_attachment_description.setSamples(vk::SampleCountFlagBits::e1);

			// color
			m_vk_attachment_description.setLoadOp(vk::AttachmentLoadOp::eClear);
			m_vk_attachment_description.setStoreOp(vk::AttachmentStoreOp::eStore);

			// stencil
			m_vk_attachment_description.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
			m_vk_attachment_description.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);

			// layout
			m_vk_attachment_description.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal);
			m_vk_attachment_description.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal);
		}

		// color blend
		{
			m_vk_color_blend_attachment_state.setColorWriteMask(vk::ColorComponentFlagBits::eR |
																vk::ColorComponentFlagBits::eG |
																vk::ColorComponentFlagBits::eB |
																vk::ColorComponentFlagBits::eA);
			m_vk_color_blend_attachment_state.setBlendEnable(false);
		}
	}

	void init_present(const std::vector<const vk::ImageView *> & image_views)
	{
		m_vk_image_views = image_views;

		// attachment description
		{
			// format
			m_vk_attachment_description.setFormat(vk::Format::eB8G8R8A8Unorm);

			// samples
			m_vk_attachment_description.setSamples(vk::SampleCountFlagBits::e1);

			// color
			m_vk_attachment_description.setLoadOp(vk::AttachmentLoadOp::eClear);
			m_vk_attachment_description.setStoreOp(vk::AttachmentStoreOp::eStore);

			// stencil
			m_vk_attachment_description.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
			m_vk_attachment_description.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);

			// layout
			m_vk_attachment_description.setInitialLayout(vk::ImageLayout::eUndefined);
			m_vk_attachment_description.setFinalLayout(vk::ImageLayout::ePresentSrcKHR);
		}

		// color blend
		{
			m_vk_color_blend_attachment_state.setColorWriteMask(vk::ColorComponentFlagBits::eR |
																vk::ColorComponentFlagBits::eG |
																vk::ColorComponentFlagBits::eB |
																vk::ColorComponentFlagBits::eA);
			m_vk_color_blend_attachment_state.setBlendEnable(false);
		}
	}
};

struct DepthAttachment
{
	vk::AttachmentDescription	m_vk_attachment_description;
	vk::ClearDepthStencilValue	m_vk_clear_depth_stencil_value;
	vk::AttachmentReference		m_vk_depth_attachment_ref;
	const vk::ImageView *		m_vk_image_view;
	bool						m_use_depth_test = true;
	bool						m_use_depth_write = true;

	DepthAttachment(const vk::ImageView * vk_image_view)
	{
		m_vk_image_view = vk_image_view;

		// depth attachment description
		{
			m_vk_attachment_description.setFormat(vk::Format::eD32Sfloat);
			m_vk_attachment_description.setSamples(vk::SampleCountFlagBits::e1);

			// depth
			m_vk_attachment_description.setLoadOp(vk::AttachmentLoadOp::eClear);
			m_vk_attachment_description.setStoreOp(vk::AttachmentStoreOp::eDontCare);

			// stencil
			m_vk_attachment_description.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
			m_vk_attachment_description.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);

			// layout
			m_vk_attachment_description.setInitialLayout(vk::ImageLayout::eUndefined);
			m_vk_attachment_description.setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
		}

		// depth attachment ref
		{
			m_vk_depth_attachment_ref.setAttachment(1);
			m_vk_depth_attachment_ref.setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
		}
	}
};
