#pragma once

#include "dxacommandlist.h"
#include "dxacommon.h"
#include "dxadevice.h"

namespace DXA_NAME
{
struct CommandPool
{
    ComPtr<ID3D12CommandAllocator> m_dx_command_allocator;
    D3D12_COMMAND_LIST_TYPE        m_command_list_type;
    std::vector<CommandList>       m_pre_alloc_cmd_lists;
    size_t                         m_cmd_buffer_index = 0;
    const Device *                 m_device;

    CommandPool() {}

    CommandPool(const Device * device, const CommandQueueType queue_type)
    : m_device(device), m_command_list_type(static_cast<D3D12_COMMAND_LIST_TYPE>(queue_type))
    {
        device->m_dx_device->CreateCommandAllocator(m_command_list_type, IID_PPV_ARGS(&m_dx_command_allocator));
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
            ID3D12CommandQueue * cmd_queue = m_device->m_dx_direct_queue.Get();
            m_pre_alloc_cmd_lists.emplace_back(cmd_queue, cmd_list);
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
};
} // namespace Dxa