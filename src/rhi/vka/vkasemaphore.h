#pragma once

#include "vkacommon.h"

#ifdef USE_VKA

#include "vkadevice.h"

namespace VKA_NAME
{
struct Semaphore
{
    vk::UniqueSemaphore m_vk_semaphore;

    Semaphore() {}

    Semaphore(const std::string & name, const Device & device)
    {
        vk::SemaphoreCreateInfo semaphore_ci = {};
        m_vk_semaphore = device.m_vk_ldevice->createSemaphoreUnique(semaphore_ci);
        device.name_vkhpp_object<vk::Semaphore, vk::Semaphore::CType>(m_vk_semaphore.get(), name);
    }
};
} // namespace VKA_NAME
#endif