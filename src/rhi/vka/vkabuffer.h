#pragma once

#include "vkacommon.h"
#include "vkadevice.h"

namespace VKA_NAME
{
struct Buffer
{
    struct VmaBufferBundle
    {
        VmaAllocator m_vma_allocator;
        VmaAllocation m_vma_allocation;
        VmaAllocationInfo m_vma_alloc_info;
        VkBuffer m_vk_buffer = nullptr;
    };

    struct VmaBufferBundleDeleter
    {
        VmaBufferBundleDeleter() {}
        void
        operator()(VmaBufferBundle bundle)
        {
            vmaDestroyBuffer(bundle.m_vma_allocator, bundle.m_vk_buffer, bundle.m_vma_allocation);
        }
    };

    UniqueVarHandle<VmaBufferBundle, VmaBufferBundleDeleter> m_vma_buffer_bundle;
    size_t m_size_in_bytes;
    vk::DeviceAddress m_device_address = std::numeric_limits<vk::DeviceAddress>::max();

    Buffer() {}

    Buffer(const Device * device,
           const BufferUsageEnum buffer_usage_,
           const MemoryUsageEnum memory_usage,
           const size_t buffer_size_in_bytes,
           const std::string & name                   = "")
    : m_size_in_bytes(buffer_size_in_bytes)
    {
        BufferUsageEnum buffer_usage = buffer_usage_;

        vk::BufferCreateInfo buffer_ci_tmp;
        buffer_ci_tmp.setSize(buffer_size_in_bytes == 0 ? 32 : buffer_size_in_bytes);
        buffer_ci_tmp.setUsage(static_cast<vk::BufferUsageFlagBits>(buffer_usage) |
                               vk::BufferUsageFlagBits::eShaderDeviceAddress);
        VkBufferCreateInfo buffer_ci         = buffer_ci_tmp;
        VmaAllocationCreateInfo vma_alloc_ci = {};
        vma_alloc_ci.usage                   = static_cast<VmaMemoryUsage>(memory_usage);

        VkBuffer vma_vk_buffer;
        VmaAllocation vma_allocation;
        VmaAllocationInfo vma_alloc_info;
        VKCK(vmaCreateBuffer(*device->m_vma_allocator, &buffer_ci, &vma_alloc_ci, &vma_vk_buffer, &vma_allocation, &vma_alloc_info));

        VmaBufferBundle vma_buffer_bundle;
        vma_buffer_bundle.m_vk_buffer      = vma_vk_buffer;
        vma_buffer_bundle.m_vma_allocation = vma_allocation;
        vma_buffer_bundle.m_vma_alloc_info = vma_alloc_info;
        vma_buffer_bundle.m_vma_allocator  = *device->m_vma_allocator;
        m_vma_buffer_bundle                = vma_buffer_bundle;

        // get device address
        vk::BufferDeviceAddressInfo device_address_info;
        device_address_info.setBuffer(vk::Buffer(m_vma_buffer_bundle->m_vk_buffer));
        m_device_address = device->m_vk_ldevice->getBufferAddress(device_address_info);

        device->name_vkhpp_object<vk::Buffer, vk::Buffer::CType>(vk::Buffer(vma_vk_buffer), name);
    }

    bool
    is_initialized()
    {
        return !m_vma_buffer_bundle.empty();
    }

    bool
    is_empty()
    {
        return m_vma_buffer_bundle.empty();
    }

    void *
    map()
    {
        void * mapped_data     = m_vma_buffer_bundle->m_vma_alloc_info.pMappedData;
        bool directly_mappable = mapped_data != nullptr;
        if (!directly_mappable)
        {
            VkResult result = vmaMapMemory(m_vma_buffer_bundle->m_vma_allocator,
                                           m_vma_buffer_bundle->m_vma_allocation,
                                           &mapped_data);
            if (result != VK_SUCCESS)
            {
                return nullptr;
            }
        }
        return mapped_data;
    }

    void
    unmap()
    {
        if (m_vma_buffer_bundle->m_vma_alloc_info.pMappedData == nullptr)
        {
            vmaUnmapMemory(m_vma_buffer_bundle->m_vma_allocator, m_vma_buffer_bundle->m_vma_allocation);
        }
    }
};
} // namespace VKA_NAME