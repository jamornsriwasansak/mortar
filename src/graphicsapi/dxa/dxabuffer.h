#pragma once

#include "dxacommon.h"
#include "dxadevice.h"
#include "dxastagingbuffermanager.h"

namespace Dxa
{
struct Buffer
{
    BufferUsageEnum                    m_buffer_usage              = BufferUsageEnum::None;
    MemoryUsageEnum                    m_memory_usage              = MemoryUsageEnum::None;
    D3D12MAHandle<D3D12MA::Allocation> m_allocation                = nullptr;
    size_t                             m_size_in_bytes             = 0;
    D3D12_GPU_DESCRIPTOR_HANDLE        m_dx_cbv_srv_uav_gpu_handle = { 0 };

    Buffer() {}

    Buffer(const Device *               device,
           const BufferUsageEnum  buffer_usage_,
           const MemoryUsageEnum  memory_usage,
           const size_t           buffer_size,
           const std::byte *      initial_data        = nullptr,
           StagingBufferManager * initial_data_loader = nullptr,
           const std::string &    name                = "")
    : m_size_in_bytes(buffer_size), m_memory_usage(memory_usage), m_buffer_usage(buffer_usage_)
    {
        BufferUsageEnum buffer_usage = buffer_usage_;
        D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
        if (HasFlag(buffer_usage, BufferUsageEnum::StorageBuffer))
        {
            flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            buffer_usage ^= BufferUsageEnum::StorageBuffer;
        }
        if (HasFlag(buffer_usage, BufferUsageEnum::RayTracingAccelStructBuffer))
        {
            flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        }

        // constant buffer needs 256 bytes alignment
        if ((buffer_usage & BufferUsageEnum::ConstantBuffer) == BufferUsageEnum::ConstantBuffer)
        {
            m_size_in_bytes = round_up(m_size_in_bytes, static_cast<size_t>(256));
        }

        // allocate memory for buffer using D3D12MA
        D3D12MA::ALLOCATION_DESC alloc_desc = {};

        if (HasFlag(buffer_usage, BufferUsageEnum::RayTracingAccelStructBuffer))
        {
            // TODO:: check once in a while if this is fixed.
            // for some reasons, the amd's D3D12MA causes bug when using FLAG_NONE to create raytracing accel creation
            alloc_desc.Flags = D3D12MA::ALLOCATION_FLAG_COMMITTED;
        }
        else
        {
            alloc_desc.Flags = D3D12MA::ALLOCATION_FLAG_NONE;
        }

        switch (memory_usage)
        {
        case MemoryUsageEnum::None:
            break;
        case MemoryUsageEnum::CpuOnly:
        case MemoryUsageEnum::CpuToGpu:
            alloc_desc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
            break;
        case MemoryUsageEnum::GpuOnly:
            alloc_desc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
            break;
        case MemoryUsageEnum::GpuToCpu:
            alloc_desc.HeapType = D3D12_HEAP_TYPE_READBACK;
            break;
        default:
            Logger::Critical<true>("Does not support MemoryUsageEnum : ", static_cast<int>(memory_usage));
            break;
        }

        // setup state
        D3D12_RESOURCE_STATES states = static_cast<D3D12_RESOURCE_STATES>(buffer_usage);
        switch (memory_usage)
        {
        case MemoryUsageEnum::CpuOnly:
        case MemoryUsageEnum::CpuToGpu:
            states |= D3D12_RESOURCE_STATE_GENERIC_READ;
            break;
        default:
            break;
        }

        D3D12_RESOURCE_STATES tmp_initial_states = states;
        if (initial_data && alloc_desc.HeapType != D3D12_HEAP_TYPE_UPLOAD)
        {
            tmp_initial_states = D3D12_RESOURCE_STATE_COPY_DEST;
        }

        // allocate resource
        ID3D12Resource *      resource;
        D3D12MA::Allocation * allocation  = nullptr;
        CD3DX12_RESOURCE_DESC buffer_desc = CD3DX12_RESOURCE_DESC::Buffer(m_size_in_bytes, flags);
        DXCK(device->m_d3d12ma->CreateResource(&alloc_desc,
                                               &buffer_desc,
                                               tmp_initial_states,
                                               nullptr,
                                               &allocation,
                                               IID_PPV_ARGS(&resource)));

        // check if allocation has problem
        if (allocation == nullptr)
        {
            Logger::Critical<true>(__FUNCTION__, " ", name.c_str(), " allocation is nullptr");
        }

        // set struct variable
        m_allocation = D3D12MAHandle<D3D12MA::Allocation>(allocation);

        // copy initial data
        if (initial_data && buffer_size > 0)
        {
            if (alloc_desc.HeapType == D3D12_HEAP_TYPE_UPLOAD)
            {
                // is upload buffer, simply use memcpy
                void * mapped_result;
                DXCK(m_allocation->GetResource()->Map(0, nullptr, &mapped_result));
                std::memcpy(mapped_result, initial_data, buffer_size);
            }
            else
            {
                // update resource
                assert(initial_data_loader);
                auto * staging_buffer =
                    initial_data_loader->get_staging_buffer(get_required_staging_size_in_bytes());
                assert(buffer_size <= staging_buffer->m_size_in_bytes);

                // is not upload buffer, need staging buffer from resource loader
                ID3D12GraphicsCommandList4 * cmd_list = initial_data_loader->m_dx_command_list.Get();
                D3D12_SUBRESOURCE_DATA       subresource_data;
                {
                    subresource_data.pData      = reinterpret_cast<const BYTE *>(initial_data);
                    subresource_data.RowPitch   = buffer_size;
                    subresource_data.SlicePitch = buffer_size;
                }
                [[maybe_unused]] UINT64 size = UpdateSubresources(cmd_list,
                                                                  m_allocation->GetResource(),
                                                                  staging_buffer->m_allocation->GetResource(),
                                                                  0,
                                                                  0,
                                                                  1,
                                                                  &subresource_data);

                // transition buffer resource states
                D3D12_RESOURCE_BARRIER barrier = {};
                barrier.Transition.pResource   = m_allocation->GetResource();
                barrier.Transition.StateBefore = tmp_initial_states;
                barrier.Transition.StateAfter  = states;
                barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                cmd_list->ResourceBarrier(1, &barrier);
            }
        }

        // set name
        device->name_dx_object(resource, name);
    }

    void *
    map()
    {
        assert(m_memory_usage == MemoryUsageEnum::CpuOnly || m_memory_usage == MemoryUsageEnum::CpuToGpu);
        void * mapped_result;
        DXCK(m_allocation->GetResource()->Map(0, nullptr, &mapped_result));
        return mapped_result;
    }

    void
    unmap()
    {
        m_allocation->GetResource()->Unmap(0, nullptr);
    }

    size_t
    get_required_staging_size_in_bytes() const
    {
        return GetRequiredIntermediateSize(m_allocation->GetResource(), 0, 1);
    }
};
} // namespace Dxa
