#pragma once

#include "dxacommandlist.h"
#include "dxacommon.h"
#include "dxadevice.h"

namespace Dxa
{
struct FlightControl
{
    // command lists
    std::vector<ComPtr<ID3D12CommandAllocator>> m_dx_command_allocators;
    CommandList                                 m_cmd_list;
    size_t                                      m_num_images = 0;

    FlightControl(Device * device, const size_t num_flights) : m_num_images(num_flights)
    {
        // init_swapchain(window);
        // init_swapchain_rtv(num_flights);
        init_command_allocators(device);
        init_command_list(device);
    }

    void
    advance_frame2(size_t index)
    {
        // reset command allocator
        ID3D12CommandAllocator * direct_command_allocator = m_dx_command_allocators[index].Get();
        DXCK(direct_command_allocator->Reset());

        // reset command list
        DXCK(m_cmd_list.m_dx_cmd_list->Reset(direct_command_allocator, nullptr));
    }

    void
    init_command_allocators(Device * device)
    {
        // create command allocator per frame
        m_dx_command_allocators.resize(m_num_images);
        for (uint32_t i_frame = 0; i_frame < m_num_images; i_frame++)
        {
            DXCK(device->m_dx_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                             IID_PPV_ARGS(&m_dx_command_allocators[i_frame])));
        }
    }

    void
    init_command_list(Device * device)
    {
        DXCK(device->m_dx_device->CreateCommandList(0,
                                                    D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                    m_dx_command_allocators[0].Get(),
                                                    NULL,
                                                    IID_PPV_ARGS(&m_cmd_list.m_dx_cmd_list)));
        DXCK(m_cmd_list.m_dx_cmd_list->Close());
        m_cmd_list.m_dx_cmd_queue = device->m_dx_direct_queue.Get();
    }
};
} // namespace Dxa