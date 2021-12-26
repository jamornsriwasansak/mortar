#pragma once

#include "pch/pch.h"
#include "rhi/common/rhi_enums.h"

namespace RhiCommon
{
template <typename FormatEnum, typename TextureUsageEnum, typename TCreateInfo>
struct TTextureCreateInfo
{
    uint32_t         m_width;
    uint32_t         m_height;
    uint16_t         m_depth;
    uint32_t         m_mip_levels;
    FormatEnum       m_format;
    TextureUsageEnum m_texture_usage;

    TTextureCreateInfo(const uint32_t         width,
                       const uint32_t         height,
                       const uint16_t         depth,
                       const size_t           mip_levels,
                       const FormatEnum       format,
                       const TextureUsageEnum texture_usage)
    : m_width(width), m_height(height), m_depth(depth), m_mip_levels(mip_levels), m_format(format), m_texture_usage(texture_usage)
    {
    }

    TCreateInfo
    get_create_info() const
    {
        if constexpr (std::is_same<TCreateInfo, vk::ImageCreateInfo>::value)
        {
            // Assign create info
            vk::ImageCreateInfo image_ci;
            image_ci.setImageType(vk::ImageType::e2D);
            image_ci.extent.setWidth(m_width);
            image_ci.extent.setHeight(m_height);
            image_ci.extent.setDepth(m_depth);
            image_ci.setMipLevels(m_mip_levels);
            image_ci.setArrayLayers(1);
            image_ci.setFormat(static_cast<vk::Format>(m_format));
            image_ci.setTiling(vk::ImageTiling::eOptimal);
            image_ci.setInitialLayout(vk::ImageLayout::eUndefined);
            image_ci.setUsage(static_cast<vk::ImageUsageFlagBits>(m_texture_usage));
            image_ci.setSamples(vk::SampleCountFlagBits::e1);
            image_ci.setSharingMode(vk::SharingMode::eExclusive);
            return image_ci;
        }
        else if constexpr (std::is_same<TCreateInfo, D3D12_RESOURCE_DESC>::value)
        {
            D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;

            // Check if UAV aka storage image
            if (HasFlag(m_texture_usage, DXA_NAME::TextureUsageEnum::StorageImage))
            {
                flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            }

            // Check is depth attachment
            if (HasFlag(m_texture_usage, DXA_NAME::TextureUsageEnum::DepthAttachment))
            {
                flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
            }

            // Check is color attachment
            if (HasFlag(m_texture_usage, DXA_NAME::TextureUsageEnum::ColorAttachment))
            {
                flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
                assert(!HasFlag(m_texture_usage, DXA_NAME::TextureUsageEnum::DepthAttachment));
            }

            // Assign description
            D3D12_RESOURCE_DESC resource_desc = {};
            resource_desc.Dimension           = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            resource_desc.Alignment           = 0;
            resource_desc.Width               = m_width;
            resource_desc.Height              = m_height;
            resource_desc.DepthOrArraySize    = m_depth;
            resource_desc.MipLevels           = m_mip_levels;
            resource_desc.Format              = static_cast<DXGI_FORMAT>(m_format);
            resource_desc.SampleDesc.Count    = 1;
            resource_desc.SampleDesc.Quality  = 0;
            resource_desc.Layout              = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            resource_desc.Flags               = flags;

            return resource_desc;
        }
        else
        {
            static_assert(false, "unknown return create info type");
            return 0;
        }
    }
};
} // namespace RhiCommon