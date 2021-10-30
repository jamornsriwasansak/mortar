#pragma once

#include "rhi/rhi.h"

struct RenderContext
{
    Rhi::Device * m_device                               = nullptr;
    Rhi::CommandPool * m_graphics_command_pool           = nullptr;
    Rhi::DescriptorPool * m_descriptor_pool              = nullptr;
    Rhi::Semaphore * m_image_ready_semaphore             = nullptr;
    Rhi::Semaphore * m_image_presentable_semaphore       = nullptr;
    Rhi::Fence * m_flight_fence                          = nullptr;
    Rhi::Texture * m_swapchain_texture                   = nullptr;
    Rhi::StagingBufferManager * m_staging_buffer_manager = nullptr;
    Rhi::Buffer * m_dummy_buffer                         = nullptr;
    Rhi::Texture * m_dummy_texture                       = nullptr;
    Rhi::ImGuiRenderPass * m_imgui_render_pass           = nullptr;
    size_t m_flight_index                                = 0;
    size_t m_image_index                                 = 0;
};