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

    FramebufferBindings([[maybe_unused]] const std::string & name,
                        [[maybe_unused]] const Device &      device,
                        const std::vector<const Texture *> & colors,
                        const Texture *                      depth = nullptr)
    : m_color_attachments(colors.begin(), colors.end())
    {
        if (depth)
        {
            m_depth_attachment = depth;
        }
    }
};
} // namespace DXA_NAME
#endif