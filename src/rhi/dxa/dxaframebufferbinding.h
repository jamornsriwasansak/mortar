#pragma once

#include "dxacommon.h"
#ifdef USE_DXA

#include "dxaswapchain.h"
#include "dxatexture.h"

namespace DXA_NAME
{
struct FramebufferBindings
{
    std::vector<const Texture *> m_color_attachments;
    const Texture *              m_depth_attachment = nullptr;

    FramebufferBindings() {}

    FramebufferBindings([[maybe_unused]] const Device *      device,
                        const std::vector<const Texture *> & colors,
                        const std::optional<const Texture *> depth = std::nullopt)
    : m_color_attachments(colors)
    {
        if (depth.has_value())
        {
            m_depth_attachment = depth.value();
        }
    }
};
} // namespace Dxa
#endif