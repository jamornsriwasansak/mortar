#pragma once

#include "dxacommon.h"
#ifdef USE_DXA

#include "dxacommandlist.h"
#include "dxadevice.h"

namespace DXA_NAME
{
struct CommandPool
{
    ComPtr<ID3D12CommandAllocator> m_dx_command_allocator = nullptr;
    ComPtr<ID3D12CommandQueue>     m_dx_command_queue     = nullptr;
    D3D12_COMMAND_LIST_TYPE        m_command_list_type    = D3D12_COMMAND_LIST_TYPE_DIRECT;
    std::vector<CommandList>       m_pre_alloc_cmd_lists  = {};
    size_t                         m_cmd_buffer_index     = 0;
    const Device *                 m_device               = nullptr;
    std::string                    m_name                 = {};

    CommandPool() {}

    CommandPool(const std::string & name, const Device * device, const CommandQueueType command_queue_type)
    : m_device(device),
      m_command_list_type(static_cast<D3D12_COMMAND_LIST_TYPE>(command_queue_type)),
      m_name(name)
    {
        device->m_dx_device->CreateCommandAllocator(m_command_list_type, IID_PPV_ARGS(&m_dx_command_allocator));
        switch (command_queue_type)
        {
        case CommandQueueType::Graphics:
            m_dx_command_queue = device->m_dx_direct_queue;
            break;
        case CommandQueueType::Compute:
            m_dx_command_queue = device->m_dx_compute_queue;
            break;
        case CommandQueueType::Transfer:
            m_dx_command_queue = device->m_dx_copy_queue;
            break;
        default:
            Logger::Critical<true>(__FUNCTION__, " reach end switch case for command_queue_type");
            break;
        }

        if (!name.empty())
        {
            device->name_dx_object(m_dx_command_queue, name);
        }
    }

    CommandList
    get_command_list()
    {
        if (m_cmd_buffer_index == m_pre_alloc_cmd_lists.size())
        {
            ComPtr<ID3D12GraphicsCommandList4> cmd_list;
            DXCK(m_device->m_dx_device->CreateCommandList(0,
                                                          m_command_list_type,
                                                          m_dx_command_allocator.Get(),
                                                          NULL,
                                                          IID_PPV_ARGS(&cmd_list)));
            // DXCK(cmd_list->Close());
            m_pre_alloc_cmd_lists.emplace_back(m_dx_command_queue.Get(), cmd_list);

            if (!m_name.empty())
            {
                m_device->name_dx_object(cmd_list, m_name + "_list_" + std::to_string(m_cmd_buffer_index));
            }
        }
        return m_pre_alloc_cmd_lists[m_cmd_buffer_index++];
    }

    void
    reset()
    {
        m_cmd_buffer_index = 0;
        DXCK(m_dx_command_allocator->Reset());
        for (size_t i = 0; i < m_pre_alloc_cmd_lists.size(); i++)
        {
            m_pre_alloc_cmd_lists[i].m_dx_cmd_list->Reset(m_dx_command_allocator.Get(), nullptr);
        }
    }

    void
    flush(Fence & fence)
    {
        fence.reset();
        m_dx_command_queue->Signal(fence.m_dx_fence.Get(), fence.m_expected_fence_value);
    }
};
} // namespace DXA_NAME
#endif