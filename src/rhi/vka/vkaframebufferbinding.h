#pragma once

#include "vkacommon.h"

#ifdef USE_VKA

#include "vkadevice.h"
#include "vkatexture.h"

namespace VKA_NAME
{
struct FramebufferBindings
{
    // note:: this struct contains both vk_render_pass and vk_framebuffer
    // it's unlikely that you will use different framebuffer with same render pass

    vk::UniqueRenderPass  m_vk_render_pass;
    vk::UniqueFramebuffer m_vk_framebuffer;
    int2                  m_resolution = int2(0, 0);

    FramebufferBindings(const std::string &                    name,
                        const Device &                         device,
                        const std::span<const Texture * const> colors,
                        const Texture * const                  depth = nullptr)
    {
        const size_t num_depth_attachment = depth ? 1u : 0u;
        const size_t num_attachments      = colors.size() + num_depth_attachment;
        size_t       i_attachment         = 0u;
        std::vector<vk::AttachmentDescription> descs(num_attachments);
        std::vector<vk::ImageView>             image_views(num_attachments);
        std::vector<vk::AttachmentReference>   color_refs(colors.size());
        std::optional<vk::AttachmentReference> depth_ref;

        int2 resolution(0, 0);

        for (size_t i = 0; i < colors.size(); i++)
        {
            const auto cattach = colors[i];
            const auto loadop  = vk::AttachmentLoadOp::eLoad;

            vk::ImageLayout attachment_layout    = cattach->get_attachment_image_layout(false);
            vk::ImageLayout initial_image_layout = vk::ImageLayout::eUndefined;
            if constexpr (loadop == vk::AttachmentLoadOp::eLoad)
            {
                initial_image_layout = attachment_layout;
            }

            vk::AttachmentReference & ref = color_refs[i];
            ref.setAttachment(static_cast<uint32_t>(i));
            ref.setLayout(attachment_layout);

            vk::AttachmentDescription & desc = descs[i_attachment];
            desc.setFormat(cattach->m_vk_format);
            desc.setSamples(vk::SampleCountFlagBits::e1);
            desc.setLoadOp(loadop);
            desc.setStoreOp(vk::AttachmentStoreOp::eStore);
            desc.setInitialLayout(initial_image_layout);
            desc.setFinalLayout(attachment_layout);

            resolution = cattach->m_resolution;

            // setup image view
            image_views[i_attachment] = cattach->m_vk_image_view.get();

            i_attachment++;
        }

        if (depth)
        {
            const auto dattach = depth;
            const auto loadop  = vk::AttachmentLoadOp::eLoad;

            vk::ImageLayout attachment_layout = dattach->get_attachment_image_layout(false);
            vk::ImageLayout initial_image_layout =
                loadop == vk::AttachmentLoadOp::eLoad ? attachment_layout : vk::ImageLayout::eUndefined;

            vk::AttachmentReference ref = {};
            ref.setAttachment(static_cast<uint32_t>(num_attachments - 1));
            ref.setLayout(attachment_layout);
            depth_ref = ref;

            vk::AttachmentDescription & desc = descs[i_attachment];
            desc.setFormat(dattach->m_vk_format);
            desc.setSamples(vk::SampleCountFlagBits::e1);
            desc.setInitialLayout(initial_image_layout);
            desc.setFinalLayout(attachment_layout);
            // depth
            desc.setLoadOp(loadop);
            desc.setStoreOp(vk::AttachmentStoreOp::eStore);
            // stencil
            desc.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
            desc.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);

            resolution = dattach->m_resolution;

            // setup image view
            image_views[i_attachment] = dattach->m_vk_image_view.get();

            i_attachment++;
        }
        assert(resolution.x > 0 && resolution.y > 0);
        assert(i_attachment == num_attachments);

        vk::AccessFlags dst_access_flag;
        if (color_refs.size() > 0)
        {
            dst_access_flag = dst_access_flag | vk::AccessFlagBits::eColorAttachmentWrite;
        }
        if (depth_ref.has_value())
        {
            dst_access_flag = dst_access_flag | vk::AccessFlagBits::eDepthStencilAttachmentWrite;
        }

        vk::SubpassDescription subpass_desc = {};
        subpass_desc.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
        subpass_desc.setColorAttachmentCount(static_cast<uint32_t>(color_refs.size()));
        subpass_desc.setPColorAttachments(color_refs.data());
        if (depth_ref.has_value())
        {
            subpass_desc.setPDepthStencilAttachment(&depth_ref.value());
        }

        // https://gpuopen.com/learn/vulkan-barriers-explained/
        // https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples
        vk::SubpassDependency subpass_dependency = {};

        subpass_dependency.setSrcSubpass(VK_SUBPASS_EXTERNAL);
        subpass_dependency.setSrcAccessMask(vk::AccessFlags());
        subpass_dependency.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);

        subpass_dependency.setDstSubpass(0);
        subpass_dependency.setDstAccessMask(dst_access_flag);
        subpass_dependency.setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);

        vk::RenderPassCreateInfo render_pass_ci = {};
        render_pass_ci.setAttachmentCount(static_cast<uint32_t>(descs.size()));
        render_pass_ci.setPAttachments(descs.data());
        render_pass_ci.setSubpassCount(1);
        render_pass_ci.setPSubpasses(&subpass_desc);
        render_pass_ci.setDependencyCount(1u);
        render_pass_ci.setPDependencies(&subpass_dependency);

        m_vk_render_pass = device.m_vk_ldevice->createRenderPassUnique(render_pass_ci);
        device.name_vkhpp_object<vk::RenderPass, vk::RenderPass::CType>(m_vk_render_pass.get(),
                                                                        name + "_render_pass");

        vk::FramebufferCreateInfo framebuffer_ci = {};
        framebuffer_ci.setRenderPass(m_vk_render_pass.get());
        framebuffer_ci.setAttachmentCount(static_cast<uint32_t>(num_attachments));
        framebuffer_ci.setPAttachments(image_views.data());
        framebuffer_ci.setWidth(resolution.x);
        framebuffer_ci.setHeight(resolution.y);
        framebuffer_ci.setLayers(1u);

        m_vk_framebuffer = device.m_vk_ldevice->createFramebufferUnique(framebuffer_ci);
        device.name_vkhpp_object<vk::Framebuffer, vk::Framebuffer::CType>(m_vk_framebuffer.get(),
                                                                          name + "_framebuffer");

        m_resolution = resolution;
    }
};
} // namespace VKA_NAME
#endif