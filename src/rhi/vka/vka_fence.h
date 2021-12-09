#pragma once

#include "vka_common.h"

#ifdef USE_VKA

    #include "vka_device.h"

namespace VKA_NAME
{
struct Fence
{
    vk::UniqueFence m_vk_fence;
    const Device &  m_device;

    Fence(const std::string & name, const Device & device) : m_device(device)
    {
        // create fence
        vk::FenceCreateInfo fence_ci = {};
        // if (is_signaled)
        {
            fence_ci.setFlags(vk::FenceCreateFlagBits::eSignaled);
        }
        m_vk_fence = device.m_vk_ldevice->createFenceUnique(fence_ci);

        // name our stuff
        device.name_vkhpp_object<vk::Fence, vk::Fence::CType>(m_vk_fence.get(), name);
    }

    void
    wait(const std::chrono::nanoseconds duration = std::chrono::nanoseconds::max())
    {
        VKCK(m_device.m_vk_ldevice->waitForFences(1,
                                                  &m_vk_fence.get(),
                                                  VK_TRUE,
                                                  static_cast<uint64_t>(duration.count())));
    }

    void
    reset()
    {
        VKCK(m_device.m_vk_ldevice->resetFences(1, &m_vk_fence.get()));
    }
};
} // namespace VKA_NAME
#endif