#pragma once

#include "pch/pch.h"

#ifdef USE_DXA

    #include "dxa_command_buffer.h"
    #include "dxa_common.h"
    #include "dxa_device.h"

namespace DXA_NAME
{
struct CommandPool
{
    ComPtr<ID3D12CommandQueue> m_dx_command_queue      = nullptr;
    D3D12_COMMAND_LIST_TYPE    m_command_list_type     = D3D12_COMMAND_LIST_TYPE_DIRECT;
    std::vector<CommandBuffer> m_pre_alloc_cmd_buffers = {};
    size_t                     m_cmd_buffer_index      = 0;
    const Device &             m_device;
    std::string                m_name;

    CommandPool(const std::string & name, const Device & device, const CommandQueueType command_queue_type)
    : m_device(device),
      m_command_list_type(static_cast<D3D12_COMMAND_LIST_TYPE>(command_queue_type)),
      m_name(name)
    {
        switch (command_queue_type)
        {
        case CommandQueueType::Graphics:
            m_dx_command_queue = device.m_dx_direct_queue;
            break;
        case CommandQueueType::Compute:
            m_dx_command_queue = device.m_dx_compute_queue;
            break;
        case CommandQueueType::Transfer:
            m_dx_command_queue = device.m_dx_copy_queue;
            break;
        default:
            Logger::Critical<true>(__FUNCTION__, " reach end switch case for command_queue_type");
            break;
        }

        device.name_dx_object(m_dx_command_queue, name);
    }

    CommandBuffer
    get_command_buffer()
    {
        if (m_cmd_buffer_index == m_pre_alloc_cmd_buffers.size())
        {
            // Create both Graphics Command List and Command Allocator
            ComPtr<ID3D12GraphicsCommandList4> cmd_list;
            ComPtr<ID3D12CommandAllocator>     cmd_allocator;
            DXCK(m_device.m_dx_device->CreateCommandAllocator(m_command_list_type, IID_PPV_ARGS(&cmd_allocator)));
            DXCK(m_device.m_dx_device->CreateCommandList(0, m_command_list_type, cmd_allocator.Get(), NULL, IID_PPV_ARGS(&cmd_list)));

            // Name both objects
            m_device.name_dx_object(cmd_list, m_name + "_list_" + std::to_string(m_cmd_buffer_index));
            m_device.name_dx_object(cmd_list, m_name + "_allocator_" + std::to_string(m_cmd_buffer_index));

            // Move both created Graphics Command List and Command Allocator into command_buffer
            m_pre_alloc_cmd_buffers.emplace_back(m_dx_command_queue.Get(), cmd_list, cmd_allocator);
        }
        return m_pre_alloc_cmd_buffers[m_cmd_buffer_index++];
    }

    void
    reset()
    {
        for (size_t i = 0; i < m_cmd_buffer_index; i++)
        {
            CommandBuffer & buffer = m_pre_alloc_cmd_buffers[i];

            // Reset the Command Allocator
            DXCK(buffer.m_dx_command_allocator->Reset());

            // Reset the Command List
            DXCK(buffer.m_dx_command_list->Reset(buffer.m_dx_command_allocator.Get(), nullptr));
        }
        m_cmd_buffer_index = 0;
    }

    void
    flush(Fence & fence)
    {
        fence.reset();
        DXCK(m_dx_command_queue->Signal(fence.m_dx_fence.Get(), fence.m_expected_fence_value));
    }
};
} // namespace DXA_NAME
#endif