#pragma once

#include "pch/pch.h"

#ifdef USE_VKA

    #include "vka_common.h"
    #include "vka_device.h"

namespace VKA_NAME
{
struct DescriptorPool
{
    vk::UniqueDescriptorPool m_vk_descriptor_pool;
    const Device &           m_device;

    DescriptorPool(const std::string & name, const Device & device) : m_device(device)
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
        m_vk_descriptor_pool = device.m_vk_ldevice->createDescriptorPoolUnique(pool_ci);

        m_device.name_vkhpp_object<vk::DescriptorPool, vk::DescriptorPool::CType>(
            m_vk_descriptor_pool.get(),
            name);
    }

    void
    reset()
    {
        m_device.m_vk_ldevice->resetDescriptorPool(m_vk_descriptor_pool.get());
    }
};
} // namespace VKA_NAME
#endif