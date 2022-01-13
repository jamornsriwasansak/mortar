#pragma once

#include "pch/pch.h"
#include "rhi/common/rhi_enums.h"

namespace Rhi
{
struct TextureCreateInfo
{
    uint32_t              m_width;
    uint32_t              m_height;
    uint16_t              m_depth;
    uint32_t              m_mip_levels;
    Rhi::FormatEnum       m_format;
    Rhi::TextureUsageEnum m_texture_usage;

    TextureCreateInfo(const uint32_t width, const uint32_t height, const Rhi::FormatEnum format, const Rhi::TextureUsageEnum texture_usage)
    : m_width(width), m_height(height), m_depth(1), m_mip_levels(1), m_format(format), m_texture_usage(texture_usage)
    {
    }

    TextureCreateInfo(const uint32_t              width,
                      const uint32_t              height,
                      const uint16_t              depth,
                      const uint32_t              mip_levels,
                      const Rhi::FormatEnum       format,
                      const Rhi::TextureUsageEnum texture_usage)
    : m_width(width), m_height(height), m_depth(depth), m_mip_levels(mip_levels), m_format(format), m_texture_usage(texture_usage)
    {
    }
};
} // namespace Rhi