#pragma once

#include "vkacommon.h"

#ifdef USE_VKA

#include "vkadevice.h"
#include "vkasemaphore.h"

namespace VKA_NAME
{
struct Swapchain
{
    // swapchain properties info
    // used in create_*_swapchain
    inline static const std::set<vk::Format> SupportedLdrSwapchainFormats = { vk::Format::eB8G8R8A8Unorm };
    inline static const std::set<vk::ColorSpaceKHR> SupportedLdrSwapchainColorSpaces = { vk::ColorSpaceKHR::eSrgbNonlinear };
    inline static const std::set<vk::PresentModeKHR> SupportedSwapchainPresentMode = { vk::PresentModeKHR::eMailbox };

    vk::UniqueSwapchainKHR m_vk_swapchain;

    size_t m_num_images = 0;
    int2   m_resolution = { 0, 0 };

    const Device & m_device;
    vk::Format     m_vk_format;

    std::vector<vk::UniqueSemaphore> is_image_acquired_semaphore;
    std::vector<vk::UniqueSemaphore> is_swapchain_image_ready_for_present_semaphores;

    size_t      m_flight      = 0;
    uint32_t    m_image_index = 0;
    std::string m_name        = "";

    Swapchain(const std::string & name, const Device & device, const Window & window)
    : m_device(device), m_name(name)
    {
        init(device, window);
    }

    void
    update_image_index(const Semaphore * image_ready)
    {
        vk::Semaphore semaphore;
        if (image_ready) semaphore = image_ready->m_vk_semaphore.get();

        uint32_t   i_image;
        vk::Result is_image_acquired =
            m_device.m_vk_ldevice->acquireNextImageKHR(*m_vk_swapchain,
                                                       std::numeric_limits<uint64_t>::max(),
                                                       semaphore,
                                                       {},
                                                       &i_image);

        if (is_image_acquired != vk::Result::eSuccess)
            Logger::Error<true>(__FUNCTION__, " acquireNextImage yields error ", vk::to_string(is_image_acquired));

        m_image_index = i_image;
    }

    bool
    present(const Semaphore * semaphore)
    {
        vk::PresentInfoKHR present_info = {};
        if (semaphore)
        {
            present_info.setPWaitSemaphores(&semaphore->m_vk_semaphore.get());
            present_info.setWaitSemaphoreCount(1u);
        }
        present_info.setPSwapchains(&m_vk_swapchain.get());
        present_info.setSwapchainCount(1u);
        present_info.setPImageIndices(&m_image_index);

        vk::Result result = m_device.m_vk_present_queue.presentKHR(&present_info);
        return result == vk::Result::eSuccess;
    }

    void
    resize_to_window(const Device & device, const Window & window)
    {
        init(device, window);
    }

private:
    void
    init(const Device & device, const Window & window)
    {
        // based on surface capabilities find out minimum number of images + 1, we can put in the
        // swapchain also get surface transform
        const auto capabilities = device.m_vk_pdevice.getSurfaceCapabilitiesKHR(device.m_vk_surface);
        const uint32_t num_min_swapchain_images = capabilities.minImageCount;
        const uint32_t num_max_swapchain_images =
            capabilities.maxImageCount == 0 ? std::numeric_limits<uint32_t>::max() : capabilities.maxImageCount;
        const uint32_t num_swapchain_images = std::min(num_min_swapchain_images + 1, num_max_swapchain_images);

        // get format, color_space and present mode
        auto [format, color_space] =
            get_surface_info(device, device.m_vk_surface, SupportedLdrSwapchainFormats, SupportedLdrSwapchainColorSpaces);
        vk::PresentModeKHR present_mode =
            get_present_mode(device, device.m_vk_surface, SupportedSwapchainPresentMode);

        // create family indices
        const uint32_t queue_family_indices[] = { device.m_family_indices.m_graphics,
                                                  device.m_family_indices.m_present };

        // get resolution
        int2 resolution = window.get_resolution();

        // create swapchain
        vk::SwapchainCreateInfoKHR swapchain_ci = {};
        swapchain_ci.setSurface(device.m_vk_surface);
        swapchain_ci.setMinImageCount(num_swapchain_images);
        swapchain_ci.setImageFormat(format);
        swapchain_ci.setImageColorSpace(color_space);
        swapchain_ci.setImageExtent(vk::Extent2D(resolution.x, resolution.y));
        swapchain_ci.setImageArrayLayers(1);
        swapchain_ci.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment);
        if (device.m_family_indices.m_graphics == device.m_family_indices.m_present)
        {
            swapchain_ci.setImageSharingMode(vk::SharingMode::eExclusive);
        }
        else
        {
            swapchain_ci.setImageSharingMode(vk::SharingMode::eConcurrent);
            swapchain_ci.setQueueFamilyIndexCount(2); // present queue and graphics queue
            swapchain_ci.setPQueueFamilyIndices(queue_family_indices);
        }
        swapchain_ci.setPreTransform(capabilities.currentTransform);
        swapchain_ci.setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque);
        swapchain_ci.setPresentMode(present_mode);
        swapchain_ci.setClipped(true);
        swapchain_ci.setOldSwapchain(*m_vk_swapchain);
        m_resolution   = window.get_resolution();
        m_vk_swapchain = device.m_vk_ldevice->createSwapchainKHRUnique(swapchain_ci);
        m_vk_format    = format;
        m_num_images   = static_cast<uint32_t>(num_swapchain_images);

        // name our stuff
        device.name_vkhpp_object<vk::SwapchainKHR, vk::SwapchainKHR::CType>(m_vk_swapchain.get(), m_name);
    }

    std::tuple<vk::Format, vk::ColorSpaceKHR>
    get_surface_info(const Device &                      device,
                     const vk::SurfaceKHR &              surface,
                     const std::set<vk::Format> &        supported_swapchain_formats,
                     const std::set<vk::ColorSpaceKHR> & supported_swapchain_color_spaces)
    {
        bool found_surface_format = false;

        // check surface formats
        vk::Format        m_swapchain_format;
        vk::ColorSpaceKHR m_swapchain_color_space;
        {
            const auto surface_formats = device.m_vk_pdevice.getSurfaceFormatsKHR(surface);
            for (const vk::SurfaceFormatKHR & surface_format : surface_formats)
            {
                if (supported_swapchain_formats.count(surface_format.format) > 0 &&
                    supported_swapchain_color_spaces.count(surface_format.colorSpace) > 0)
                {
                    found_surface_format    = true;
                    m_swapchain_format      = surface_format.format;
                    m_swapchain_color_space = surface_format.colorSpace;
                    break;
                }
            }
            if (!found_surface_format)
            {
                Logger::Critical<true>(
                    __FUNCTION__ " could not find specified surface format or colorspace");
            }
        }
        return std::make_tuple(m_swapchain_format, m_swapchain_color_space);
    }

    vk::PresentModeKHR
    get_present_mode(const Device &                       device,
                     const vk::SurfaceKHR &               surface,
                     const std::set<vk::PresentModeKHR> & supported_swapchain_present_mode)
    {
        const auto present_modes = device.m_vk_pdevice.getSurfacePresentModesKHR(surface);
        bool       found_wanted_present_mode = false;
        for (const vk::PresentModeKHR & present_mode : present_modes)
        {
            if (supported_swapchain_present_mode.count(present_mode) > 0)
            {
                found_wanted_present_mode = true;
                return present_mode;
            }
        }
        if (!found_wanted_present_mode)
        {
            Logger::Critical<true>(__FUNCTION__ " could not find specified present mode");
        }
        return vk::PresentModeKHR();
    }
};
} // namespace VKA_NAME
#endif