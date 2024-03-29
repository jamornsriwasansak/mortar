#pragma once

#include "pch/pch.h"

#ifdef USE_DXA

    #include "rhi/common/rhi_enums.h"
    #include "rhi/common/rhi_shader_src.h"
    #include "rhi/common/rhi_texture_create_info.h"
    //
    #include "core/glfwhandler.h"
    #include "core/logger.h"
//#define USE_NSIGHT_AFTERMATH

    #define DXCK(ResultEvalExpression)                                                       \
        do                                                                                   \
        {                                                                                    \
            HRESULT result571481963 = ResultEvalExpression;                                  \
            if (FAILED(result571481963))                                                     \
            {                                                                                \
                _com_error err571481963(result571481963);                                    \
                LPCTSTR    err_msg571481963 = err571481963.ErrorMessage();                   \
                Logger::Error<true>(__FUNCTION__, " : ", __LINE__, " : ", err_msg571481963); \
            }                                                                                \
        } while (false)

namespace DXA_NAME
{
template <typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

template <typename Type>
struct D3D12MAObjectDeleter
{
    D3D12MAObjectDeleter() {}
    void
    operator()(Type * data)
    {
        data->Release();
    }
};
template <typename Type>
using D3D12MAHandle = std::unique_ptr<Type, D3D12MAObjectDeleter<Type>>;

struct DescriptorHandle
{
    D3D12_GPU_DESCRIPTOR_HANDLE m_dx_gpu_handle = { 0 };
    D3D12_CPU_DESCRIPTOR_HANDLE m_dx_cpu_handle = { 0 };
    size_t                      num_handles     = 0;
};

struct DescriptorGpuCpuHandle
{
    D3D12_GPU_DESCRIPTOR_HANDLE m_dx_gpu_handle_start = { 0 };
    D3D12_CPU_DESCRIPTOR_HANDLE m_dx_cpu_handle_start = { 0 };

    void
    initialize(ID3D12DescriptorHeap * heap)
    {
        m_dx_gpu_handle_start = heap->GetGPUDescriptorHandleForHeapStart();
        m_dx_cpu_handle_start = heap->GetCPUDescriptorHandleForHeapStart();
    }

    DescriptorHandle
    offset(const UINT offset) const
    {
        DescriptorHandle handle;
        handle.m_dx_cpu_handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_dx_cpu_handle_start, offset);
        handle.m_dx_gpu_handle = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_dx_gpu_handle_start, offset);
        return handle;
    }
};

struct DescriptorCpuHandle
{
    D3D12_CPU_DESCRIPTOR_HANDLE m_dx_cpu_handle_start = { 0 };

    void
    initialize(ID3D12DescriptorHeap * heap)
    {
        m_dx_cpu_handle_start = heap->GetCPUDescriptorHandleForHeapStart();
    }

    DescriptorHandle
    offset(const UINT offset) const
    {
        DescriptorHandle handle;
        handle.m_dx_cpu_handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_dx_cpu_handle_start, offset);
        return handle;
    }
};

template <typename THandleStart>
struct DescriptorHeap
{
    ID3D12Device5 *              m_dx_device          = nullptr;
    ComPtr<ID3D12DescriptorHeap> m_dx_descriptor_heap = nullptr;
    UINT                         m_dx_offset          = 0;
    UINT                         m_dx_handle_size     = 0;
    THandleStart                 m_handle_start;

    DescriptorHeap() {}

    DescriptorHeap(ID3D12Device5 *             dx_device,
                   D3D12_DESCRIPTOR_HEAP_FLAGS heap_flags,
                   D3D12_DESCRIPTOR_HEAP_TYPE  heap_type,
                   const UINT                  num_descriptors)
    : m_dx_device(dx_device)
    {
        D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {};
        heap_desc.NumDescriptors             = num_descriptors;
        heap_desc.Flags                      = heap_flags;
        heap_desc.Type                       = heap_type;
        DXCK(m_dx_device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(m_dx_descriptor_heap.GetAddressOf())));
        m_handle_start.initialize(m_dx_descriptor_heap.Get());
        m_dx_handle_size = m_dx_device->GetDescriptorHandleIncrementSize(heap_type);
        m_dx_offset      = 0;
    }

    DescriptorHandle
    request_handle(const size_t num_handles)
    {
        DescriptorHandle handle = m_handle_start.offset(m_dx_offset);
        handle.num_handles      = num_handles;
        m_dx_offset += static_cast<UINT>(m_dx_handle_size * num_handles);
        return handle;
    }

    DescriptorHandle
    get_rtv_handle(ID3D12Resource * resources, const size_t num_resources = 1)
    {
        assert(m_dx_descriptor_heap->GetDesc().Type == D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        DescriptorHandle handle = request_handle(num_resources);
        for (size_t i_handle = 0; i_handle < num_resources; i_handle++)
        {
            CD3DX12_CPU_DESCRIPTOR_HANDLE cpu_handle(handle.m_dx_cpu_handle,
                                                     static_cast<INT>(i_handle * m_dx_handle_size));
            m_dx_device->CreateRenderTargetView(&resources[i_handle], nullptr, cpu_handle);
        }
        return handle;
    }

    DescriptorHandle
    get_dsv_handle(const D3D12_DEPTH_STENCIL_VIEW_DESC & desc, ID3D12Resource * resources, const size_t num_resources = 1)
    {
        assert(m_dx_descriptor_heap->GetDesc().Type == D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
        DescriptorHandle handle = request_handle(num_resources);
        for (size_t i_handle = 0; i_handle < num_resources; i_handle++)
        {
            CD3DX12_CPU_DESCRIPTOR_HANDLE cpu_handle(handle.m_dx_cpu_handle,
                                                     static_cast<INT>(i_handle * m_dx_handle_size));
            m_dx_device->CreateDepthStencilView(&resources[i_handle], &desc, cpu_handle);
        }
        return handle;
    }

    void
    reset()
    {
        m_handle_start.initialize(m_dx_descriptor_heap.Get());
        m_dx_offset = 0;
    }
};
} // namespace DXA_NAME
#endif
