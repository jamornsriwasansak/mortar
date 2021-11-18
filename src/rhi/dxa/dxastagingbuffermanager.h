#pragma once

#include "dxacommon.h"
#ifdef USE_DXA

#include "dxadevice.h"

namespace DXA_NAME
{
struct StagingBufferManager
{
    // TODO:: build unique handle and set m_is_available to true once the unique handle is destroyed
    struct StagingBuffer
    {
        D3D12MAHandle<D3D12MA::Allocation> m_allocation    = nullptr;
        bool                               m_is_available  = true;
        size_t                             m_size_in_bytes = 0;
    };

    // TODO:: build unique handle and set m_is_available to true once the unique handle is destroyed
    struct ScratchBuffer
    {
        D3D12MAHandle<D3D12MA::Allocation> m_allocation    = nullptr;
        bool                               m_is_available  = true;
        size_t                             m_size_in_bytes = 0;
    };

    ComPtr<ID3D12CommandAllocator>     m_direct_command_allocator = nullptr;
    ComPtr<ID3D12GraphicsCommandList4> m_dx_command_list          = nullptr;
    Device *                           m_device                   = nullptr;
    D3D12MA::Allocator *               m_d3d12ma                  = nullptr;
    std::list<StagingBuffer>           m_staging_buffers;
    std::list<ScratchBuffer>           m_scratch_buffers;
    static constexpr size_t            DefaultStagingBufferSize = 100 * 1024 * 1024; // 100 MB
    std::string                        m_name                   = "";

    StagingBufferManager() {}

    StagingBufferManager(Device * device, const std::string & name = "")
    {
        // create global copy command allocator
        DXCK(device->m_dx_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                         IID_PPV_ARGS(&m_direct_command_allocator)));
        // create command list
        DXCK(device->m_dx_device->CreateCommandList(0,
                                                    D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                    m_direct_command_allocator.Get(),
                                                    nullptr,
                                                    IID_PPV_ARGS(&m_dx_command_list)));

        m_name    = name;
        m_device  = device;
        m_d3d12ma = device->m_d3d12ma.get();

        // set name
        device->name_dx_object(m_dx_command_list, name + "_cmdalloc");
        device->name_dx_object(m_direct_command_allocator, name + "_cmdlist");
    }

    void
    submit_all_pending_upload()
    {
        m_dx_command_list->Close();

        // execute command lists
        ID3D12CommandList * pp_command_lists[] = { m_dx_command_list.Get() };
        m_device->m_dx_direct_queue->ExecuteCommandLists(1, pp_command_lists);

        // submit fence value
        ComPtr<ID3D12Fence> fence;
        DXCK(m_device->m_dx_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
        DXCK(m_device->m_dx_direct_queue->Signal(fence.Get(), 1));

        m_device->name_dx_object(fence, m_name + "_fence");

        // wait
        HANDLE fence_handle = ::CreateEvent(NULL, FALSE, FALSE, NULL);
        DXCK(fence->SetEventOnCompletion(1, fence_handle));
        ::WaitForSingleObject(fence_handle, INFINITE);

        m_dx_command_list->Reset(m_direct_command_allocator.Get(), nullptr);

        // set everything to true since we already submitted everything
        for (StagingBuffer & staging_buffer : m_staging_buffers)
        {
            staging_buffer.m_is_available = true;
        }

        for (ScratchBuffer & scratch_buffer : m_scratch_buffers)
        {
            scratch_buffer.m_is_available = true;
        }
    }

    StagingBuffer *
    get_staging_buffer(const size_t required_size_in_bytes)
    {
        // find available staging buffer in the list and return
        for (StagingBuffer & staging_buffer : m_staging_buffers)
        {
            if (staging_buffer.m_is_available)
            {
                staging_buffer.m_is_available = false;
                return &staging_buffer;
            }
        }

        // create new staging buffer and return
        StagingBuffer            staging_buffer;
        ID3D12Resource *         resource   = nullptr;
        D3D12MA::Allocation *    allocation = nullptr;
        D3D12MA::ALLOCATION_DESC alloc_desc = {};
        alloc_desc.Flags                    = D3D12MA::ALLOCATION_FLAG_NONE;
        alloc_desc.HeapType                 = D3D12_HEAP_TYPE_UPLOAD;
        CD3DX12_RESOURCE_DESC buffer_desc =
            CD3DX12_RESOURCE_DESC::Buffer(DefaultStagingBufferSize, D3D12_RESOURCE_FLAG_NONE);
        m_d3d12ma->CreateResource(&alloc_desc,
                                  &buffer_desc,
                                  D3D12_RESOURCE_STATE_GENERIC_READ | D3D12_RESOURCE_STATE_COPY_SOURCE,
                                  nullptr,
                                  &allocation,
                                  IID_PPV_ARGS(&resource));
        staging_buffer.m_allocation    = D3D12MAHandle<D3D12MA::Allocation>(allocation);
        staging_buffer.m_size_in_bytes = DefaultStagingBufferSize;
        staging_buffer.m_is_available  = false;

        // set name
        m_device->name_dx_object(resource, m_name + "_staging_buffer");

        assert(staging_buffer.m_size_in_bytes >= required_size_in_bytes);

        m_staging_buffers.push_back(std::move(staging_buffer));

        // sanity check if we used too many staging buffers or not
        assert(m_staging_buffers.size() < 5);

        return &(m_staging_buffers.back());
    }

    ScratchBuffer *
    get_scratch_buffer(const size_t required_size_in_bytes)
    {
        // find available staging buffer in the list and return
        for (ScratchBuffer & scratch_buffer : m_scratch_buffers)
        {
            if (scratch_buffer.m_is_available)
            {
                scratch_buffer.m_is_available = false;
                return &scratch_buffer;
            }
        }

        // create new scratch buffer
        ScratchBuffer            scratch_buffer;
        ID3D12Resource *         resource   = nullptr;
        D3D12MA::Allocation *    allocation = nullptr;
        D3D12MA::ALLOCATION_DESC alloc_desc = {};
        alloc_desc.Flags                    = D3D12MA::ALLOCATION_FLAG_NONE;
        alloc_desc.HeapType                 = D3D12_HEAP_TYPE_DEFAULT;
        CD3DX12_RESOURCE_DESC buffer_desc =
            CD3DX12_RESOURCE_DESC::Buffer(required_size_in_bytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        m_d3d12ma->CreateResource(&alloc_desc,
                                  &buffer_desc,
                                  D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                  nullptr,
                                  &allocation,
                                  IID_PPV_ARGS(&resource));
        scratch_buffer.m_allocation    = D3D12MAHandle<D3D12MA::Allocation>(allocation);
        scratch_buffer.m_size_in_bytes = required_size_in_bytes;
        scratch_buffer.m_is_available  = false;

        // set name
        m_device->name_dx_object(resource, m_name + "_scratch_buffer");

        assert(scratch_buffer.m_size_in_bytes >= required_size_in_bytes);
        m_scratch_buffers.push_back(std::move(scratch_buffer));

        assert(m_scratch_buffers.size() < 5);

        return &(m_scratch_buffers.back());
    }
};
} // namespace Dxa
#endif