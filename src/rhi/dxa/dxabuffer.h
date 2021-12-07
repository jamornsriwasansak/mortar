#pragma once

#include "pch/pch.h"

#ifdef USE_DXA

    #include "dxacommon.h"
    #include "dxadevice.h"

namespace DXA_NAME
{
struct Buffer
{
    BufferUsageEnum                    m_buffer_usage              = BufferUsageEnum::None;
    MemoryUsageEnum                    m_memory_usage              = MemoryUsageEnum::None;
    D3D12MAHandle<D3D12MA::Allocation> m_allocation                = nullptr;
    size_t                             m_size_in_bytes             = 0;
    D3D12_GPU_DESCRIPTOR_HANDLE        m_dx_cbv_srv_uav_gpu_handle = { 0 };

    Buffer() {}

    Buffer(const std::string &   name,
           const Device &        device,
           const BufferUsageEnum buffer_usage,
           const MemoryUsageEnum memory_usage,
           const size_t          buffer_size_in_bytes)
    : m_size_in_bytes(buffer_size_in_bytes), m_memory_usage(memory_usage), m_buffer_usage(buffer_usage)
    {
        BufferUsageEnum      modified_buffer_usage = buffer_usage;
        D3D12_RESOURCE_FLAGS flags                 = D3D12_RESOURCE_FLAG_NONE;
        if (HasFlag(modified_buffer_usage, BufferUsageEnum::StorageBuffer))
        {
            flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            modified_buffer_usage ^= BufferUsageEnum::StorageBuffer;
            assert(memory_usage == MemoryUsageEnum::GpuOnly);
        }
        if (HasFlag(modified_buffer_usage, BufferUsageEnum::RayTracingAccelStructBuffer))
        {
            flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            assert(memory_usage == MemoryUsageEnum::GpuOnly);
        }

        // constant buffer needs 256 bytes alignment
        if ((modified_buffer_usage & BufferUsageEnum::ConstantBuffer) == BufferUsageEnum::ConstantBuffer)
        {
            m_size_in_bytes = round_up(m_size_in_bytes, static_cast<size_t>(256));
        }

        // allocate memory for buffer using D3D12MA
        D3D12MA::ALLOCATION_DESC alloc_desc = {};

        if (HasFlag(modified_buffer_usage, BufferUsageEnum::RayTracingAccelStructBuffer))
        {
            // TODO:: check once in a while if this is fixed.
            // for some reasons, the amd's D3D12MA causes bug when using FLAG_NONE to create raytracing accel creation
            alloc_desc.Flags = D3D12MA::ALLOCATION_FLAG_COMMITTED;
        }
        else
        {
            // alloc_desc.Flags = D3D12MA::ALLOCATION_FLAG_NONE;
            alloc_desc.Flags = D3D12MA::ALLOCATION_FLAG_COMMITTED;
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
        D3D12_RESOURCE_STATES states = static_cast<D3D12_RESOURCE_STATES>(modified_buffer_usage);
        if (!HasOnlyFlag(modified_buffer_usage, BufferUsageEnum::StorageBuffer))
        {
            states = states & ~D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        }
        if (!HasOnlyFlag(modified_buffer_usage, BufferUsageEnum::VertexBuffer))
        {
            states = states & ~D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        }
        if (!HasOnlyFlag(modified_buffer_usage, BufferUsageEnum::IndexBuffer))
        {
            states = states & ~D3D12_RESOURCE_STATE_INDEX_BUFFER;
        }

        switch (memory_usage)
        {
        case MemoryUsageEnum::CpuOnly:
        case MemoryUsageEnum::CpuToGpu:
            states |= D3D12_RESOURCE_STATE_GENERIC_READ;
            break;
        default:
            break;
        }

        // allocate resource
        ID3D12Resource *      resource;
        D3D12MA::Allocation * allocation  = nullptr;
        CD3DX12_RESOURCE_DESC buffer_desc = CD3DX12_RESOURCE_DESC::Buffer(m_size_in_bytes, flags);
        DXCK(device.m_d3d12ma->CreateResource(&alloc_desc, &buffer_desc, states, nullptr, &allocation, IID_PPV_ARGS(&resource)));

        // check if allocation has problem
        if (allocation == nullptr)
        {
            Logger::Critical<true>(__FUNCTION__, " ", name.c_str(), " allocation is nullptr");
        }

        // set struct variable
        m_allocation = D3D12MAHandle<D3D12MA::Allocation>(allocation);

        // set name
        device.name_dx_object(resource, name);
    }

    void *
    map()
    {
        assert(m_memory_usage == MemoryUsageEnum::CpuOnly || m_memory_usage == MemoryUsageEnum::CpuToGpu ||
               m_memory_usage == MemoryUsageEnum::GpuToCpu);
        void * mapped_result;
        DXCK(m_allocation->GetResource()->Map(0, nullptr, &mapped_result));
        return mapped_result;
    }

    bool
    is_initialized()
    {
        return m_allocation == nullptr;
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
} // namespace DXA_NAME

#endif
