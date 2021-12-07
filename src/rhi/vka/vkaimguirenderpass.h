#pragma once

#include "vkacommon.h"

#ifdef USE_VKA

#include "pch/pch.h"

#include "vkadevice.h"
#include "vkaswapchain.h"
#include "vkatexture.h"

namespace VKA_NAME
{
struct ImGuiRenderPass
{
    vk::UniqueRenderPass               m_vk_render_pass;
    std::vector<vk::UniqueImageView>   m_vk_image_views;
    std::vector<vk::UniqueFramebuffer> m_vk_framebuffers;
    int2                               m_resolution;
    vk::UniqueDescriptorPool           m_descriptor_pool = {};

    ImGuiRenderPass(const std::string & name, const Device & device, const Window & window, const Swapchain & swapchain)
    {
        vk::AttachmentDescription attachment = {};
        attachment.setFormat(swapchain.m_vk_format);
        attachment.setSamples(vk::SampleCountFlagBits::e1);
        attachment.setLoadOp(vk::AttachmentLoadOp::eLoad);
        attachment.setStoreOp(vk::AttachmentStoreOp::eStore);
        attachment.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
        attachment.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
        attachment.setInitialLayout(vk::ImageLayout::eUndefined);
        attachment.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal);

        vk::AttachmentReference color_attachment = {};
        color_attachment.setAttachment(0);
        color_attachment.setLayout(vk::ImageLayout::eColorAttachmentOptimal);

        vk::SubpassDescription subpass = {};
        subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
        subpass.setColorAttachmentCount(1);
        subpass.setPColorAttachments(&color_attachment);

        vk::SubpassDependency dependency = {};
        dependency.setSrcSubpass(VK_SUBPASS_EXTERNAL);
        dependency.setDstSubpass(0);
        dependency.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
        dependency.setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
        dependency.setSrcAccessMask(vk::AccessFlags());
        dependency.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);

        vk::RenderPassCreateInfo info = {};
        info.setAttachmentCount(1);
        info.setPAttachments(&attachment);
        info.setSubpassCount(1);
        info.setPSubpasses(&subpass);
        info.setDependencyCount(1);
        info.setPDependencies(&dependency);
        m_vk_render_pass = device.m_vk_ldevice->createRenderPassUnique(info);
        device.name_vkhpp_object<vk::RenderPass, vk::RenderPass::CType>(m_vk_render_pass.get(), name);

        ImGuiIO & io = ImGui::GetIO();

        if (io.BackendPlatformUserData == NULL)
        {
            ImGui_ImplGlfw_InitForVulkan(window.m_glfw_window, true);
        }

        // create descriptor pool
        std::array<vk::DescriptorPoolSize, 5> pool_sizes;
        pool_sizes[0].setDescriptorCount(5);
        pool_sizes[0].setType(vk::DescriptorType::eUniformBuffer);
        pool_sizes[1].setDescriptorCount(5);
        pool_sizes[1].setType(vk::DescriptorType::eStorageBuffer);
        pool_sizes[2].setDescriptorCount(5);
        pool_sizes[2].setType(vk::DescriptorType::eAccelerationStructureKHR);
        pool_sizes[3].setDescriptorCount(5);
        pool_sizes[3].setType(vk::DescriptorType::eSampler);
        pool_sizes[4].setDescriptorCount(5);
        pool_sizes[4].setType(vk::DescriptorType::eStorageImage);
        vk::DescriptorPoolCreateInfo pool_ci;
        pool_ci.setPoolSizes(pool_sizes);
        pool_ci.setMaxSets(100);
        m_descriptor_pool = device.m_vk_ldevice->createDescriptorPoolUnique(pool_ci);
        device.name_vkhpp_object<vk::DescriptorPool, vk::DescriptorPool::CType>(m_descriptor_pool.get(), name);

        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance                  = static_cast<VkInstance>(device.m_vk_instance);
        init_info.PhysicalDevice            = static_cast<VkPhysicalDevice>(device.m_vk_pdevice);
        init_info.Device                    = static_cast<VkDevice>(device.m_vk_ldevice.get());
        init_info.QueueFamily               = device.m_family_indices.m_graphics;
        init_info.Queue                     = static_cast<VkQueue>(device.m_vk_graphics_queue);
        init_info.PipelineCache             = VK_NULL_HANDLE;
        init_info.DescriptorPool  = static_cast<VkDescriptorPool>(m_descriptor_pool.get());
        init_info.Allocator       = nullptr;
        init_info.MinImageCount   = static_cast<uint32_t>(swapchain.m_num_images);
        init_info.ImageCount      = static_cast<uint32_t>(swapchain.m_num_images);
        init_info.CheckVkResultFn = nullptr;
        ImGui_ImplVulkan_Init(&init_info, static_cast<VkRenderPass>(m_vk_render_pass.get()));

        device.one_time_command_submit([&](vk::CommandBuffer cmd_buf) {
            ImGui_ImplVulkan_CreateFontsTexture(static_cast<VkCommandBuffer>(cmd_buf));
        });

        init_or_resize_framebuffer(device, swapchain);
    }

    vk::UniqueImageView
    create_image_view(const vk::Device device, const vk::Image image, const vk::Format format)
    {
        vk::ImageViewCreateInfo image_view_ci;
        image_view_ci.setImage(image);
        image_view_ci.setViewType(vk::ImageViewType::e2D);
        image_view_ci.setFormat(format);
        image_view_ci.components.setR(vk::ComponentSwizzle::eIdentity);
        image_view_ci.components.setG(vk::ComponentSwizzle::eIdentity);
        image_view_ci.components.setB(vk::ComponentSwizzle::eIdentity);
        image_view_ci.components.setA(vk::ComponentSwizzle::eIdentity);
        image_view_ci.subresourceRange.setAspectMask(vk::ImageAspectFlagBits::eColor);
        image_view_ci.subresourceRange.setBaseMipLevel(0);
        image_view_ci.subresourceRange.setLevelCount(1);
        image_view_ci.subresourceRange.setBaseArrayLayer(0);
        image_view_ci.subresourceRange.setLayerCount(1);
        return device.createImageViewUnique(image_view_ci);
    }

    void
    init_or_resize_framebuffer(const Device & device, const Swapchain & swapchain)
    {
        auto swapchain_images = device.m_vk_ldevice->getSwapchainImagesKHR(*swapchain.m_vk_swapchain);
        m_vk_framebuffers.clear();
        m_vk_image_views.clear();
        m_resolution = swapchain.m_resolution;
        for (uint32_t i = 0; i < swapchain.m_num_images; i++)
        {
            vk::ImageView       image_views[1];
            vk::UniqueImageView image_view =
                create_image_view(device.m_vk_ldevice.get(), swapchain_images[i], swapchain.m_vk_format);
            image_views[0] = image_view.get();

            vk::FramebufferCreateInfo framebuffer_ci = {};
            framebuffer_ci.setRenderPass(m_vk_render_pass.get());
            framebuffer_ci.setAttachmentCount(1);
            framebuffer_ci.setPAttachments(&(image_views[0]));
            framebuffer_ci.width  = m_resolution.x;
            framebuffer_ci.height = m_resolution.y;
            framebuffer_ci.layers = 1;

            m_vk_framebuffers.push_back(device.m_vk_ldevice->createFramebufferUnique(framebuffer_ci));
            m_vk_image_views.push_back(std::move(image_view));
        }
    }

    void
    new_frame() const
    {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void
    end_frame() const
    {
        ImGui::EndFrame();
    }

    void
    shut_down() const
    {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
};
} // namespace VKA_NAME
#endif