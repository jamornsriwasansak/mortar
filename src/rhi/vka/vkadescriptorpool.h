#pragma once

#include "vkacommon.h"

#ifdef USE_VKA

#include "vkadevice.h"

namespace VKA_NAME
{
struct DescriptorPool
{
    vk::UniqueDescriptorPool m_vk_descriptor_pool;
    const Device *           m_device;

    DescriptorPool() {}

    DescriptorPool(const Device * device): m_device(device)
    {
        std::array<vk::DescriptorPoolSize, 5> pool_sizes;
        pool_sizes[0].setDescriptorCount(100);
        pool_sizes[0].setType(vk::DescriptorType::eUniformBuffer);
        pool_sizes[1].setDescriptorCount(100);
        pool_sizes[1].setType(vk::DescriptorType::eStorageBuffer);
        pool_sizes[2].setDescriptorCount(100);
        pool_sizes[2].setType(vk::DescriptorType::eAccelerationStructureKHR);
        pool_sizes[3].setDescriptorCount(100);
        pool_sizes[3].setType(vk::DescriptorType::eSampler);
        pool_sizes[4].setDescriptorCount(100);
        pool_sizes[4].setType(vk::DescriptorType::eStorageImage);

        vk::DescriptorPoolCreateInfo pool_ci;
        pool_ci.setPoolSizes(pool_sizes);
        pool_ci.setMaxSets(100);
        m_vk_descriptor_pool = device->m_vk_ldevice->createDescriptorPoolUnique(pool_ci);
    }

    void
    reset()
    {
        m_device->m_vk_ldevice->resetDescriptorPool(m_vk_descriptor_pool.get());
    }
};
} // namespace VKA_NAME
#endif