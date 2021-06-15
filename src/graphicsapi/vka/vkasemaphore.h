#pragma once

#include "vkacommon.h"
#include "vkadevice.h"

namespace Vka
{
struct Semaphore
{
    vk::UniqueSemaphore m_vk_semaphore;

    Semaphore(const Device * device)
    {
        vk::SemaphoreCreateInfo semaphore_ci = {};
        m_vk_semaphore = device->m_vk_ldevice->createSemaphoreUnique(semaphore_ci);
    }
};
} // namespace Vka