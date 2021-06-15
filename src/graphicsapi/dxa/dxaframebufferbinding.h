#pragma once

#include "dxacommon.h"
#include "dxaswapchain.h"
#include "dxatexture.h"

namespace Dxa
{
struct FramebufferBindings
{
    std::vector<Texture *> m_color_attachments;
    Texture *              m_depth_attachment = nullptr;

    FramebufferBindings() {}

    FramebufferBindings(const Device *                 device,
                        const std::vector<Texture *> & colors,
                        const std::optional<Texture *> depth = std::nullopt)
    : m_color_attachments(colors)
    {
        if (depth.has_value())
        {
            m_depth_attachment = depth.value();
        }
    }
};
} // namespace Dxa