#pragma once

#include "dxa_common.h"
#ifdef USE_DXA

    #include "dxa_device.h"
    #include "dxa_stagingbuffermanager.h"
    #include "dxa_swapchain.h"

namespace DXA_NAME
{
struct Texture
{
    D3D12MAHandle<D3D12MA::Allocation> m_allocation            = nullptr;
    ID3D12Resource *                   m_dx_resource           = nullptr;
    DXGI_FORMAT                        m_dx_format             = DXGI_FORMAT_UNKNOWN;
    D3D12_CPU_DESCRIPTOR_HANDLE        m_dx_dsv_rtv_cpu_handle = { 0 };
    int2                               m_resolution            = int2(0, 0);
    float4                             m_clear_value           = float4(0.0f, 0.0f, 0.0f, 0.0f);

    Texture() {}

    Texture(const std::string & name, Device & device, const Swapchain & swapchain, const size_t i_image)
    : m_resolution(swapchain.m_resolution), m_dx_format(swapchain.m_dx_format)
    {
        m_dx_resource = swapchain.m_dx_swapchain_resource_pointers[i_image].Get();
        m_dx_format   = swapchain.m_dx_format;
        init_rtv(device);
        device.name_dx_object(m_dx_resource, name);
    }

    Texture(const std::string &    name,
            Device &               device,
            const TextureUsageEnum texture_usage,
            const TextureStateEnum initial_state,
            const FormatEnum       format,
            const int2             resolution,
            const std::byte *      initial_data        = nullptr,
            StagingBufferManager * initial_data_loader = nullptr)
    : m_resolution(resolution), m_dx_format(static_cast<DXGI_FORMAT>(format))
    {
        bool is_color_render_target = false;
        bool is_depth_render_target = false;

        D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;

        // is UAV aka storage image
        if (HasFlag(texture_usage, TextureUsageEnum::StorageImage))
        {
            flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        }

        // is depth attachment
        if (HasFlag(texture_usage, TextureUsageEnum::DepthAttachment))
        {
            is_depth_render_target = true;
            flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        }

        // is color attachment
        if (HasFlag(texture_usage, TextureUsageEnum::ColorAttachment))
        {
            is_color_render_target = true;
            flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
            assert(is_depth_render_target == false);
        }

        D3D12MA::ALLOCATION_DESC alloc_desc = {};
        alloc_desc.HeapType                 = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_RESOURCE_DESC resource_desc = {};
        resource_desc.Dimension           = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        resource_desc.Alignment           = 0;
        resource_desc.Width               = resolution.x;
        resource_desc.Height              = resolution.y;
        resource_desc.DepthOrArraySize    = 1;
        resource_desc.MipLevels           = 1;
        resource_desc.Format              = m_dx_format;
        resource_desc.SampleDesc.Count    = 1;
        resource_desc.SampleDesc.Quality  = 0;
        resource_desc.Layout              = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        resource_desc.Flags               = flags;

        // if data is provided
        D3D12_RESOURCE_STATES temp_initial_usage = static_cast<D3D12_RESOURCE_STATES>(initial_state);
        if (initial_data)
        {
            temp_initial_usage = D3D12_RESOURCE_STATE_COPY_DEST;
        }

        // allocate resource
        ID3D12Resource *      resource   = nullptr;
        D3D12MA::Allocation * allocation = nullptr;
        device.m_d3d12ma->CreateResource(&alloc_desc,
                                         &resource_desc,
                                         temp_initial_usage,
                                         nullptr,
                                         &allocation,
                                         IID_PPV_ARGS(&resource));

        // check if allocation has problem
        if (allocation == nullptr)
        {
            Logger::Critical<true>(__FUNCTION__, " ", name.c_str(), " allocation is nullptr");
        }
        m_allocation  = D3D12MAHandle<D3D12MA::Allocation>(allocation);
        m_dx_resource = resource;

        // initial with data
        if (initial_data)
        {
            const size_t size_in_bytes_per_row = resolution.x * EnumHelper::GetSizeInBytesPerPixel(format);
            const size_t size_in_bytes =
                resolution.x * resolution.y * EnumHelper::GetSizeInBytesPerPixel(format);

            assert(initial_data_loader);
            auto * staging_buffer =
                initial_data_loader->get_staging_buffer(get_required_staging_size_in_bytes());
            assert(size_in_bytes <= staging_buffer->m_size_in_bytes);

            // update subresources
            ID3D12GraphicsCommandList4 * cmd_list = initial_data_loader->m_dx_command_list.Get();
            D3D12_SUBRESOURCE_DATA       subresource_data;
            {
                subresource_data.pData      = reinterpret_cast<const BYTE *>(initial_data);
                subresource_data.RowPitch   = size_in_bytes_per_row;
                subresource_data.SlicePitch = size_in_bytes;
            }
            [[maybe_unused]] UINT64 size = UpdateSubresources(cmd_list,
                                                              m_allocation->GetResource(),
                                                              staging_buffer->m_allocation->GetResource(),
                                                              0,
                                                              0,
                                                              1,
                                                              &subresource_data);
            // assert(size == size_in_bytes);

            // transit to desired buffer_usage
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Transition.pResource   = m_allocation->GetResource();
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            barrier.Transition.StateAfter  = static_cast<D3D12_RESOURCE_STATES>(initial_state);
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            cmd_list->ResourceBarrier(1, &barrier);
        }

        if (is_color_render_target)
        {
            init_rtv(device);
        }
        else if (is_depth_render_target)
        {
            init_dsv(device, static_cast<DXGI_FORMAT>(format));
        }

        device.name_dx_object(resource, name);
    }

    bool
    is_empty() const
    {
        return m_dx_resource == nullptr;
    }

    size_t
    get_required_staging_size_in_bytes() const
    {
        return GetRequiredIntermediateSize(m_allocation->GetResource(), 0, 1);
    }

    DXGI_FORMAT
    get_view_format() const
    {
        DXGI_FORMAT mapped_format = m_dx_format;
        if (mapped_format == DXGI_FORMAT_D32_FLOAT)
        {
            mapped_format = DXGI_FORMAT_R32_FLOAT;
        }
        return mapped_format;
    }

private:
    void
    init_rtv(Device & device)
    {
        m_dx_dsv_rtv_cpu_handle = device.m_rtv_descriptor_heap.get_rtv_handle(m_dx_resource).m_dx_cpu_handle;
    }

    void
    init_dsv(Device & device, const DXGI_FORMAT format)
    {
        // create depth stencil view in descriptor heap
        D3D12_DEPTH_STENCIL_VIEW_DESC depth_stencil_desc = {};
        depth_stencil_desc.Format                        = format;
        depth_stencil_desc.ViewDimension                 = D3D12_DSV_DIMENSION_TEXTURE2D;
        depth_stencil_desc.Flags                         = D3D12_DSV_FLAG_NONE;

        m_dx_dsv_rtv_cpu_handle =
            device.m_dsv_descriptor_heap.get_dsv_handle(depth_stencil_desc, m_dx_resource).m_dx_cpu_handle;
    }
};
} // namespace DXA_NAME
#endif