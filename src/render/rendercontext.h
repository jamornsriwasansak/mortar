#pragma once

#include "graphicsapi/graphicsapi.h"

struct RenderContext
{
    Gp::Device *         m_device                       = nullptr;
    Gp::CommandPool *    m_graphics_command_pool        = nullptr;
    Gp::DescriptorPool * m_descriptor_pool              = nullptr;
    Gp::Semaphore *      m_image_ready_semaphore        = nullptr;
    Gp::Semaphore *      m_image_presentable_semaphores = nullptr;
};