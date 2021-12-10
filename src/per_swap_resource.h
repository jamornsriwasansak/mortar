#pragma once

#include "rhi/rhi.h"

struct PerSwapResource
{
    Rhi::Texture m_swapchain_texture;
    Rhi::Fence * m_swapchain_image_fence;

    PerSwapResource(const std::string & name, Rhi::Device & device, const Rhi::Swapchain & swapchain, const size_t i_image)
    : m_swapchain_texture(name + "_swapchain_texture", device, swapchain, i_image, float4(0.0f, 0.0f, 0.0f, 0.0f)),
      m_swapchain_image_fence(nullptr)
    {
    }
};