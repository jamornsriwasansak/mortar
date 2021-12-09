#pragma once

#include "pch/pch.h"

#ifdef USE_VKA

    #include "vka_command_list.h"
    #include "vka_common.h"
    #include "vka_device.h"
    #include "vka_fence.h"

namespace VKA_NAME
{
struct CommandPool
{
    const Rhi::Device &   m_device;
    vk::UniqueCommandPool m_vk_cmd_pool;
    vk::Queue             m_vk_queue;

    std::vector<CommandList> m_pre_alloc_cmd_lists;
    size_t                   m_cmd_buffer_index = 0;
    std::string              m_name;

    CommandPool(const std::string & name, const Device & device, const CommandQueueType command_queue_type)
    : m_device(device), m_name(name)
    {
        vk::CommandPoolCreateInfo command_pool_ci;
        switch (command_queue_type)
        {
        case CommandQueueType::Graphics:
            command_pool_ci.setQueueFamilyIndex(device.m_family_indices.m_graphics);
            m_vk_queue = device.m_vk_graphics_queue;
            break;
        case CommandQueueType::Compute:
            command_pool_ci.setQueueFamilyIndex(device.m_family_indices.m_compute);
            m_vk_queue = device.m_vk_compute_queue;
            break;
        case CommandQueueType::Transfer:
            command_pool_ci.setQueueFamilyIndex(device.m_family_indices.m_transfer);
            m_vk_queue = device.m_vk_transfer_queue;
            break;
        default:
            Logger::Critical<true>(__FUNCTION__, " reach end switch case for command_queue_type");
            break;
        }
        m_vk_cmd_pool = device.m_vk_ldevice->createCommandPoolUnique(command_pool_ci);

        device.name_vkhpp_object<vk::CommandPool, vk::CommandPool::CType>(m_vk_cmd_pool.get(), name);
    }

    CommandList
    get_command_list()
    {
        if (m_cmd_buffer_index == m_pre_alloc_cmd_lists.size())
        {
            vk::CommandBufferAllocateInfo allocate_info;
            allocate_info.setCommandPool(m_vk_cmd_pool.get());
            allocate_info.setCommandBufferCount(1);
            vk::CommandBuffer command_buffer =
                m_device.m_vk_ldevice->allocateCommandBuffers(allocate_info)[0];
            m_pre_alloc_cmd_lists.emplace_back(m_vk_queue, command_buffer);
        }
        return m_pre_alloc_cmd_lists[m_cmd_buffer_index++];
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