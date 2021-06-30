#pragma once

#include "vkacommon.h"
#include "vkadevice.h"
#include "vkaswapchain.h"
#include "vkatexture.h"

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"
#include "imgui.h"

namespace Vka
{
struct ImGuiRenderPass
{
    vk::UniqueDescriptorPool   m_vk_descriptor_pool;
    VkRenderPass               m_vk_render_pass;
    std::vector<VkFramebuffer> m_vk_framebuffer;
    int2                       m_resolution;

    ImGuiRenderPass() {}

    vk::ImageView
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
        return device.createImageView(image_view_ci);
    }

    ImGuiRenderPass(const Device * device, const Window & window, const Swapchain & swapchain, const size_t num_flights)
    {
        m_resolution = swapchain.m_resolution;

        std::array<vk::DescriptorPoolSize, 5> pool_sizes;
        pool_sizes[0].setDescriptorCount(100);
        pool_sizes[0].setType(vk::DescriptorType::eUniformBuffer);
        pool_sizes[1].setDescriptorCount(100);
        pool_sizes[1].setType(vk::DescriptorType::eStorageBuffer);
        pool_sizes[2].setDescriptorCount(100);
        pool_sizes[2].setType(vk::DescriptorType::eAccelerationStructureKHR);
        pool_sizes[3].setDescriptorCount(100);
        pool_sizes[3].setType(vk::DescriptorType::eSampler);
        pool_sizes[4].setDescriptorCount(100);
        pool_sizes[4].setType(vk::DescriptorType::eStorageImage);
        vk::DescriptorPoolCreateInfo pool_ci;
        pool_ci.setPoolSizes(pool_sizes);
        pool_ci.setMaxSets(100);
        m_vk_descriptor_pool = device->m_vk_ldevice->createDescriptorPoolUnique(pool_ci);

        VkAttachmentDescription attachment = {};
        attachment.format                  = static_cast<VkFormat>(swapchain.m_vk_format);
        attachment.samples                 = VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp                  = VK_ATTACHMENT_LOAD_OP_LOAD;
        attachment.storeOp                 = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.stencilLoadOp           = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp          = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
        attachment.finalLayout             = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference color_attachment = {};
        color_attachment.attachment            = 0;
        color_attachment.layout                = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments    = &color_attachment;

        VkSubpassDependency dependency = {};
        dependency.srcSubpass          = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass          = 0;
        dependency.srcStageMask        = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstStageMask        = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask       = 0; // or VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependency.dstAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo info = {};
        info.sType                  = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        info.attachmentCount        = 1;
        info.pAttachments           = &attachment;
        info.subpassCount           = 1;
        info.pSubpasses             = &subpass;
        info.dependencyCount        = 1;
        info.pDependencies          = &dependency;
        if (vkCreateRenderPass(device->m_vk_ldevice.get(), &info, nullptr, &m_vk_render_pass) != VK_SUCCESS)
        {
            throw std::runtime_error("Could not create Dear ImGui's render pass");
        }

        auto swapchain_images = device->m_vk_ldevice->getSwapchainImagesKHR(*swapchain.m_vk_swapchain);
        for (uint32_t i = 0; i < swapchain.m_num_images; i++)
        {
            VkImageView image_views[1];
            auto        image_view =
                create_image_view(device->m_vk_ldevice.get(), swapchain_images[i], swapchain.m_vk_format);
            image_views[0] = image_view;

            VkFramebufferCreateInfo framebuffer_ci = {};
            framebuffer_ci.sType                   = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebuffer_ci.renderPass              = m_vk_render_pass;
            framebuffer_ci.attachmentCount         = 1;
            framebuffer_ci.pAttachments            = &(image_views[0]);
            framebuffer_ci.width                   = m_resolution.x;
            framebuffer_ci.height                  = m_resolution.y;
            framebuffer_ci.layers                  = 1;

            VkFramebuffer fb;
            vkCreateFramebuffer(device->m_vk_ldevice.get(), &framebuffer_ci, nullptr, &fb);

            m_vk_framebuffer.push_back(fb);
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO & io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        ImGui::StyleColorsDark();

        ImGui_ImplGlfw_InitForVulkan(window.m_glfw_window, true);
        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance                  = device->m_vk_instance;
        init_info.PhysicalDevice            = device->m_vk_pdevice;
        init_info.Device                    = device->m_vk_ldevice.get();
        init_info.QueueFamily               = device->m_family_indices.m_graphics;
        init_info.Queue                     = device->m_vk_graphics_queue;
        init_info.PipelineCache             = VK_NULL_HANDLE;
        init_info.DescriptorPool            = m_vk_descriptor_pool.get();
        init_info.Allocator                 = nullptr;
        init_info.MinImageCount             = swapchain.m_num_images;
        init_info.ImageCount                = swapchain.m_num_images;
        init_info.CheckVkResultFn           = nullptr;
        ImGui_ImplVulkan_Init(&init_info, m_vk_render_pass);

        device->one_time_command_submit(
            [&](vk::CommandBuffer cmd_buf) { ImGui_ImplVulkan_CreateFontsTexture(cmd_buf); });
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
} // namespace Vka