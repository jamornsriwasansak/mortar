#pragma once

#include "dxa_common.h"
#ifdef USE_DXA

    #include "dxa_device.h"
    #include "dxa_swapchain.h"

namespace DXA_NAME
{
struct Texture
{
    int3                               m_resolution            = int3(0, 0, 0);
    DXGI_FORMAT                        m_dx_format             = DXGI_FORMAT_UNKNOWN;
    D3D12MAHandle<D3D12MA::Allocation> m_allocation            = nullptr;
    ID3D12Resource *                   m_dx_resource           = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE        m_dx_dsv_rtv_cpu_handle = { 0 };

    Texture() {}

    Texture(const std::string & name, Device & device, const Swapchain & swapchain, const size_t i_image)
    : m_resolution(int3(swapchain.m_resolution, 1)),
      m_dx_format(swapchain.m_dx_format),
      m_allocation(nullptr),
      m_dx_resource(swapchain.m_dx_swapchain_resource_pointers[i_image].Get()),
      m_dx_dsv_rtv_cpu_handle(device.m_rtv_descriptor_heap.get_rtv_handle(m_dx_resource).m_dx_cpu_handle)
    {
        device.name_dx_object(m_dx_resource, name);
    }

    Texture(const std::string & name, Device & device, const TextureCreateInfo & create_info, const TextureStateEnum & state)
    : m_resolution(int3(create_info.m_width, create_info.m_height, create_info.m_depth)),
      m_dx_format(GetDXGI_FORMAT(create_info.m_format)),
      m_allocation(ConstructAllocation(device, create_info, state)),
      m_dx_resource(m_allocation->GetResource()),
      m_dx_dsv_rtv_cpu_handle(
          ConstructInitRtvOrDsv(device, create_info.m_texture_usage, create_info.m_format, m_dx_resource))
    {
        device.name_dx_object(m_dx_resource, name);
    }

    static D3D12_RESOURCE_DESC
    GetD3D12_RESOURCE_DESC(const TextureCreateInfo & create_info)
    {
        D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;

        // Check if UAV aka storage image
        if (HasFlag(create_info.m_texture_usage, Rhi::TextureUsageEnum::StorageImage))
        {
            flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        }

        // Check is depth attachment
        if (HasFlag(create_info.m_texture_usage, Rhi::TextureUsageEnum::DepthAttachment))
        {
            flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        }

        // Check is color attachment
        if (HasFlag(create_info.m_texture_usage, Rhi::TextureUsageEnum::ColorAttachment))
        {
            flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
            assert(!HasFlag(create_info.m_texture_usage, Rhi::TextureUsageEnum::DepthAttachment));
        }

        // Assign description
        D3D12_RESOURCE_DESC resource_desc = {};
        if (create_info.m_depth > 1)
        {
            resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
        }
        else if (create_info.m_height > 1)
        {
            resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        }
        else
        {
            resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE1D;
        }
        resource_desc.Alignment          = 0;
        resource_desc.Width              = create_info.m_width;
        resource_desc.Height             = create_info.m_height;
        resource_desc.DepthOrArraySize   = create_info.m_depth;
        resource_desc.MipLevels          = create_info.m_mip_levels;
        resource_desc.Format             = GetDXGI_FORMAT(create_info.m_format);
        resource_desc.SampleDesc.Count   = 1;
        resource_desc.SampleDesc.Quality = 0;
        resource_desc.Layout             = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        resource_desc.Flags              = flags;

        return resource_desc;
    }

    static D3D12MAHandle<D3D12MA::Allocation>
    ConstructAllocation(const Device & device, const TextureCreateInfo & create_info, const TextureStateEnum & state)
    {
        // Get resource description
        D3D12_RESOURCE_DESC resource_desc = GetD3D12_RESOURCE_DESC(create_info);

        // Allocate description
        D3D12MA::ALLOCATION_DESC alloc_desc = {};
        alloc_desc.HeapType                 = D3D12_HEAP_TYPE_DEFAULT;

        // Allocate resource
        ID3D12Resource *      resource   = nullptr;
        D3D12MA::Allocation * allocation = nullptr;
        device.m_d3d12ma->CreateResource(&alloc_desc,
                                         &resource_desc,
                                         GetD3D12_RESOURCE_STATES(state),
                                         nullptr,
                                         &allocation,
                                         IID_PPV_ARGS(&resource));
        return D3D12MAHandle<D3D12MA::Allocation>(allocation);
    }

    static D3D12_CPU_DESCRIPTOR_HANDLE
    ConstructInitRtvOrDsv(Device & device, const TextureUsageEnum texture_usage, const FormatEnum format, ID3D12Resource * resource)
    {
        // Initialize RTV / DSV if is attachment
        if (HasFlag(texture_usage, TextureUsageEnum::ColorAttachment))
        {
            return device.m_rtv_descriptor_heap.get_rtv_handle(resource).m_dx_cpu_handle;
        }
        else if (HasFlag(texture_usage, TextureUsageEnum::DepthAttachment))
        {
            // Create depth stencil view in descriptor heap
            D3D12_DEPTH_STENCIL_VIEW_DESC depth_stencil_desc = {};
            depth_stencil_desc.Format                        = GetDXGI_FORMAT(format);
            depth_stencil_desc.ViewDimension                 = D3D12_DSV_DIMENSION_TEXTURE2D;
            depth_stencil_desc.Flags                         = D3D12_DSV_FLAG_NONE;

            return device.m_dsv_descriptor_heap.get_dsv_handle(depth_stencil_desc, resource).m_dx_cpu_handle;
        }

        // Don't care and return any value if not DSV / RTV
        return { 0 };
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