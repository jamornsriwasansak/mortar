#pragma once

#include "vkacommon.h"

#ifdef USE_VKA

#include "vkadevice.h"

namespace VKA_NAME
{
struct StagingBufferManager
{
    struct StagingBuffer
    {
        VmaAllocator      m_vma_allocator;
        VmaAllocation     m_vma_allocation;
        VmaAllocationInfo m_vma_alloc_info;
        VkBuffer          m_vk_buffer;
        bool              m_is_available;
    };

    struct ScratchBuffer
    {
        VmaAllocator      m_vma_allocator;
        VmaAllocation     m_vma_allocation;
        VmaAllocationInfo m_vma_alloc_info;
        VkBuffer          m_vk_buffer;
        VkDeviceAddress   m_vk_device_address;
        bool              m_is_available;
    };

    struct StagingBufferDeleter
    {
        StagingBufferDeleter() {}
        void
        operator()(StagingBuffer bundle)
        {
            vmaDestroyBuffer(bundle.m_vma_allocator, bundle.m_vk_buffer, bundle.m_vma_allocation);
        }
    };

    struct ScratchBufferDeleter
    {
        ScratchBufferDeleter() {}
        void
        operator()(ScratchBuffer bundle)
        {
            vmaDestroyBuffer(bundle.m_vma_allocator, bundle.m_vk_buffer, bundle.m_vma_allocation);
        }
    };

    std::list<UniqueVarHandle<StagingBuffer, StagingBufferDeleter>> m_staging_buffers;
    std::list<UniqueVarHandle<ScratchBuffer, ScratchBufferDeleter>> m_scratch_buffers;
    vk::UniqueCommandPool                                           m_vk_command_pool;
    vk::CommandBuffer                                               m_vk_command_buffer;
    std::string                                                     m_name;
    const Device &                                                  m_device;
    static constexpr DeviceSizeT DefaultStagingBufferSize = 100 * 1024 * 1024; // 100 MB

    StagingBufferManager(const std::string & name, const Device & device)
    : m_device(device), m_name(name)
    {
        vk::CommandPoolCreateInfo cmd_pool_ci;
        cmd_pool_ci.setQueueFamilyIndex(device.m_family_indices.m_graphics);
        cmd_pool_ci.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);

        // create pool
        m_vk_command_pool = device.m_vk_ldevice->createCommandPoolUnique(cmd_pool_ci);
        device.name_vkhpp_object<vk::CommandPool, vk::CommandPool::CType>(m_vk_command_pool.get(),
                                                                          name + "_" +
                                                                              "command_pool");

        // create command buffer
        vk::CommandBufferAllocateInfo allocate_info;
        allocate_info.setCommandPool(*m_vk_command_pool);
        allocate_info.setCommandBufferCount(1);
        auto cmd_buffers    = device.m_vk_ldevice->allocateCommandBuffers(allocate_info);
        m_vk_command_buffer = cmd_buffers.at(0);
        device.name_vkhpp_object<vk::CommandBuffer, vk::CommandBuffer::CType>(m_vk_command_buffer,
                                                                              name + "_" +
                                                                                  "command_buffer");

        // begin command buffer right away
        vk::CommandBufferBeginInfo cmd_buf_begin_info;
        cmd_buf_begin_info.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
        m_vk_command_buffer.begin(cmd_buf_begin_info);
    }

    void
    submit_all_pending_upload(const std::chrono::nanoseconds timeout = std::chrono::nanoseconds::max())
    {
        m_vk_command_buffer.end();

        // create fence
        vk::FenceCreateInfo fence_ci;
        fence_ci.setFlags(vk::FenceCreateFlagBits::eSignaled);
        vk::UniqueFence fence = m_device.m_vk_ldevice->createFenceUnique(fence_ci);
        m_device.m_vk_ldevice->resetFences({ fence.get() });
        m_device.name_vkhpp_object<vk::Fence, vk::Fence::CType>(fence.get(), m_name + "_fence");

        // execute command list
        vk::SubmitInfo submit_info;
        submit_info.setPCommandBuffers(&m_vk_command_buffer);
        submit_info.setCommandBufferCount(1);
        m_device.m_vk_graphics_queue.submit({ submit_info }, fence.get());

        // wait
        VKCK(m_device.m_vk_ldevice->waitForFences({ fence.get() }, VK_TRUE, timeout.count()));

        m_vk_command_buffer.reset();

        // begin command buffer right away
        vk::CommandBufferBeginInfo cmd_buf_begin_info;
        cmd_buf_begin_info.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
        m_vk_command_buffer.begin(cmd_buf_begin_info);

        for (auto & staging_buffer : m_staging_buffers)
        {
            staging_buffer->m_is_available = true;
        }

        for (auto & scratch_buffer : m_scratch_buffers)
        {
            scratch_buffer->m_is_available = true;
        }
    }

    StagingBuffer *
    get_staging_buffer(const DeviceSizeT required_size_in_bytes)
    {
        // find available staging buffer inthe list and return
        for (auto & staging_buffer : m_staging_buffers)
        {
            if (staging_buffer->m_is_available && required_size_in_bytes < DefaultStagingBufferSize)
            {
                staging_buffer->m_is_available = false;
                return &staging_buffer.get();
            }
        }

        assert(required_size_in_bytes < DefaultStagingBufferSize);

        // buffer create info
        vk::BufferCreateInfo buffer_ci_tmp;
        buffer_ci_tmp.setSize(DefaultStagingBufferSize);
        buffer_ci_tmp.setUsage(vk::BufferUsageFlagBits::eTransferSrc);
        VkBufferCreateInfo      buffer_ci    = buffer_ci_tmp;
        VmaAllocationCreateInfo vma_alloc_ci = {};
        vma_alloc_ci.usage                   = VmaMemoryUsage::VMA_MEMORY_USAGE_CPU_ONLY;

        // create new staging buffer and return
        VkBuffer          vma_vk_buffer;
        VmaAllocation     vma_allocation;
        VmaAllocationInfo vma_alloc_info;
        VKCK(vmaCreateBuffer(*m_device.m_vma_allocator, &buffer_ci, &vma_alloc_ci, &vma_vk_buffer, &vma_allocation, &vma_alloc_info));
        m_device.name_vkhpp_object<vk::Buffer, vk::Buffer::CType>(vk::Buffer(vma_vk_buffer),
                                                                  m_name + "_staging_buffer");

        // staging buffer
        StagingBuffer staging_buffer;
        staging_buffer.m_vma_allocator  = m_device.m_vma_allocator.get();
        staging_buffer.m_vma_allocation = vma_allocation;
        staging_buffer.m_vma_alloc_info = vma_alloc_info;
        staging_buffer.m_is_available   = false;
        staging_buffer.m_vk_buffer      = vma_vk_buffer;

        m_staging_buffers.emplace_back(staging_buffer);

        assert(m_staging_buffers.size() < 5);

        return &m_staging_buffers.back().get();
    }

    ScratchBuffer *
    get_scratch_buffer(const DeviceSizeT required_size_in_bytes)
    {
        // find available scratch buffer inthe list and reutrn
        for (auto & scratch_buffer : m_scratch_buffers)
        {
            if (scratch_buffer->m_is_available && required_size_in_bytes < DefaultStagingBufferSize)
            {
                scratch_buffer->m_is_available = false;
                return &scratch_buffer.get();
            }
        }

        assert(required_size_in_bytes < DefaultStagingBufferSize);

        // buffer create info
        vk::BufferCreateInfo buffer_ci_tmp;
        buffer_ci_tmp.setSize(DefaultStagingBufferSize);
        buffer_ci_tmp.setUsage(vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress);
        VkBufferCreateInfo      buffer_ci    = buffer_ci_tmp;
        VmaAllocationCreateInfo vma_alloc_ci = {};
        vma_alloc_ci.usage                   = VmaMemoryUsage::VMA_MEMORY_USAGE_GPU_ONLY;

        // create new staging buffer and return
        VkBuffer          vma_vk_buffer;
        VmaAllocation     vma_allocation;
        VmaAllocationInfo vma_alloc_info;
        VKCK(vmaCreateBuffer(*m_device.m_vma_allocator, &buffer_ci, &vma_alloc_ci, &vma_vk_buffer, &vma_allocation, &vma_alloc_info));
        m_device.name_vkhpp_object<vk::Buffer, vk::Buffer::CType>(vk::Buffer(vma_vk_buffer),
                                                                  m_name + "_staging_buffer");

        // get device address
        vk::BufferDeviceAddressInfo device_address_info;
        device_address_info.setBuffer(vk::Buffer(vma_vk_buffer));
        VkDeviceAddress device_address = m_device.m_vk_ldevice->getBufferAddress(device_address_info);

        // staging buffer
        ScratchBuffer scratch_buffer;
        scratch_buffer.m_vma_allocator     = m_device.m_vma_allocator.get();
        scratch_buffer.m_vma_allocation    = vma_allocation;
        scratch_buffer.m_vma_alloc_info    = vma_alloc_info;
        scratch_buffer.m_is_available      = false;
        scratch_buffer.m_vk_buffer         = vma_vk_buffer;
        scratch_buffer.m_vk_device_address = device_address;

        m_scratch_buffers.emplace_back(scratch_buffer);

        assert(m_scratch_buffers.size() < 5);

        return &m_scratch_buffers.back().get();
    }
};
} // namespace VKA_NAME
#endif