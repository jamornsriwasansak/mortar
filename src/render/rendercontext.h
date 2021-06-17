#pragma once

#include "graphicsapi/graphicsapi.h"

struct RenderContext
{
    Gp::Device *               m_device                       = nullptr;
    Gp::CommandPool *          m_graphics_command_pool        = nullptr;
    Gp::DescriptorPool *       m_descriptor_pool              = nullptr;
    Gp::Semaphore *            m_image_ready_semaphore        = nullptr;
    Gp::Semaphore *            m_image_presentable_semaphores = nullptr;
    Gp::Fence *                m_flight_fence                 = nullptr;
    Gp::Texture *              m_swapchain_texture            = nullptr;
    Gp::StagingBufferManager * m_staging_buffer_manager       = nullptr;
    size_t                     m_flight_index                 = 0;
    size_t                     m_image_index                  = 0;
};