#pragma once

#include "pch/pch.h"

#ifdef USE_VKA

    #include "vka_common.h"
    #include "vka_device.h"

namespace VKA_NAME
{
struct MemoryRequirement
{
    vk::MemoryRequirements m_vk_requirements;

    bool
    can_support(const MemoryRequirement & requirement) const
    {
        if (m_vk_requirements.alignment < requirement.m_vk_requirements.alignment)
        {
            return false;
        }
        if (m_vk_requirements.alignment % requirement.m_vk_requirements.alignment > 0)
        {
            return false;
        }
        if (m_vk_requirements.size < requirement.m_vk_requirements.size)
        {
            return false;
        }
        if ((m_vk_requirements.memoryTypeBits & requirement.m_vk_requirements.memoryTypeBits) == 0)
        {
            return false;
        }
        return true;
    }
};

struct AliasableMemory
{
    struct VmaMemoryBundle
    {
        VmaAllocator  m_vma_allocator;
        VmaAllocation m_vma_allocation;
    };

    struct VmaMemoryBundleDeleter
    {
        VmaMemoryBundleDeleter() {}

        void
        operator()(VmaMemoryBundle bundle)
        {
            vmaFreeMemory(bundle.m_vma_allocator, bundle.m_vma_allocation);
        }
    };

    UniqueVarHandle<VmaMemoryBundle, VmaMemoryBundleDeleter> m_vma_memory_bundle;
    const Device &                                           m_device;
    const MemoryRequirement                                  m_memory_requirement;
    std::string                                              m_name;

    AliasableMemory(const std::string &       name,
                    const Device &            device,
                    const MemoryRequirement & memory_requirement,
                    const MemoryUsageEnum     memory_usage)
    : m_name(name),
      m_device(device),
      m_memory_requirement(memory_requirement),
      m_vma_memory_bundle(ConstructMemoryBundle(device, memory_requirement, memory_usage))
    {
    }

    static VmaMemoryBundle
    ConstructMemoryBundle(const Device & device, const MemoryRequirement & memory_requirement, const MemoryUsageEnum memory_usage)
    {
        VmaAllocationCreateInfo alloc_ci = {};
        alloc_ci.usage                   = static_cast<VmaMemoryUsage>(memory_usage);

        VkMemoryRequirements m_vk_requirements = memory_requirement.m_vk_requirements;
        VmaAllocation        vma_allocation;
        VmaAllocationInfo    vma_alloc_info;
        vmaAllocateMemory(device.m_vma_allocator.get(), &m_vk_requirements, &alloc_ci, &vma_allocation, &vma_alloc_info);
    }

    bool
    can_support(const MemoryRequirement & requirement) const
    {
        return m_memory_requirement.can_support(requirement);
    }
};
}; // namespace VKA_NAME
#endif