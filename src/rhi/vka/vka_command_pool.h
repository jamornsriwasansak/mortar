#pragma once

#include "pch/pch.h"

#ifdef USE_VKA

    #include "vka_command_buffer.h"
    #include "vka_common.h"
    #include "vka_device.h"
    #include "vka_fence.h"

namespace VKA_NAME
{
struct CommandPool
{
    const Device &        m_device;
    vk::UniqueCommandPool m_vk_cmd_pool;
    vk::Queue             m_vk_queue;

    std::vector<CommandBuffer> m_pre_alloc_cmd_buffers;
    size_t                     m_cmd_buffer_index = 0;
    std::string                m_name;

    CommandPool(const std::string & name, const Device & device, const QueueType queue_type)
    : m_device(device), m_name(name)
    {
        vk::CommandPoolCreateInfo command_pool_ci;
        command_pool_ci.setQueueFamilyIndex(device.get_queue_family_index(queue_type));
        m_vk_queue    = device.get_queue(queue_type);
        m_vk_cmd_pool = device.m_vk_ldevice->createCommandPoolUnique(command_pool_ci);
        device.name_vkhpp_object(m_vk_cmd_pool.get(), name);
    }

    CommandBuffer
    get_command_buffer()
    {
        if (m_cmd_buffer_index == m_pre_alloc_cmd_buffers.size())
        {
            vk::CommandBufferAllocateInfo allocate_info;
            allocate_info.setCommandPool(m_vk_cmd_pool.get());
            allocate_info.setCommandBufferCount(1);
            vk::CommandBuffer command_buffer;
            VKCK(m_device.m_vk_ldevice->allocateCommandBuffers(&allocate_info, &command_buffer));
            m_pre_alloc_cmd_buffers.emplace_back(m_vk_queue, command_buffer);
        }
        return m_pre_alloc_cmd_buffers[m_cmd_buffer_index++];
    }

    void
    flush(const Fence & fence) const
    {
        vk::SubmitInfo submit_info = {};
        m_vk_queue.submit({ submit_info }, fence.m_vk_fence.get());
    }

    void
    reset()
    {
        m_device.m_vk_ldevice->resetCommandPool(m_vk_cmd_pool.get());
        m_cmd_buffer_index = 0;
    }
};
} // namespace VKA_NAME
#endif