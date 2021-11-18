#pragma once

#include "vkacommon.h"

#ifdef USE_VKA

#include "vkadevice.h"
#include "vkastagingbuffermanager.h"
#include "vkaswapchain.h"

namespace VKA_NAME
{
struct Texture
{
    struct VmaImageBundle
    {
        VmaAllocator      m_vma_allocator;
        VmaAllocation     m_vma_allocation;
        VmaAllocationInfo m_vma_alloc_info;
        VkImage           m_vk_image;
    };

    struct VmaImageBundleDeleter
    {
        VmaImageBundleDeleter() {}
        void
        operator()(VmaImageBundle bundle)
        {
            vmaDestroyImage(bundle.m_vma_allocator, bundle.m_vk_image, bundle.m_vma_allocation);
        }
    };

    int3                  m_resolution;
    vk::Image             m_vk_image = {};
    vk::UniqueImageView   m_vk_image_view;
    vk::UniqueFramebuffer m_vk_framebuffer;
    vk::ImageLayout       m_vk_image_layout;
    vk::Format            m_vk_format;
    VkMemoryPropertyFlags m_vk_mem_flag;
    float4                m_clear_value;

    UniqueVarHandle<VmaImageBundle, VmaImageBundleDeleter> m_vma_image_bundle;

    Texture() {}

    Texture(const Device *    device,
            const Swapchain & swapchain,
            const size_t      i_image,
            const float4      clear_value = float4(0.0f, 0.0f, 0.0f, 0.0f))
    : m_clear_value(clear_value)
    {
        m_vk_image = device->m_vk_ldevice->getSwapchainImagesKHR(*swapchain.m_vk_swapchain)[i_image];
        m_vk_image_view = create_image_view(device->m_vk_ldevice.get(), m_vk_image, swapchain.m_vk_format);
        m_vk_format  = swapchain.m_vk_format;
        m_resolution = int3(swapchain.m_resolution, 1);

        // convert the layout into present khr
        device->one_time_command_submit([&](vk::CommandBuffer cmd_buf) {
            vk::ImageLayout old_vk_image_layout = vk::ImageLayout::eUndefined;
            vk::ImageLayout new_vk_image_layout = vk::ImageLayout::ePresentSrcKHR;

            vk::AccessFlags        src_access_mask = access_flags_for_layout(old_vk_image_layout);
            vk::AccessFlags        dst_access_mask = access_flags_for_layout(new_vk_image_layout);
            vk::PipelineStageFlags src_pipeline_stage_flags = pipeline_stage_flags(old_vk_image_layout);
            vk::PipelineStageFlags dst_pipeline_stage_flags = pipeline_stage_flags(new_vk_image_layout);

            // setup barrier make sure that image is properly setup
            vk::ImageMemoryBarrier img_mem_barrier;
            img_mem_barrier.setOldLayout(old_vk_image_layout);
            img_mem_barrier.setNewLayout(new_vk_image_layout);
            img_mem_barrier.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
            img_mem_barrier.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
            img_mem_barrier.setImage(m_vk_image);
            img_mem_barrier.subresourceRange.setAspectMask(vk::ImageAspectFlagBits::eColor);
            img_mem_barrier.subresourceRange.setBaseMipLevel(0);
            img_mem_barrier.subresourceRange.setLevelCount(1);
            img_mem_barrier.subresourceRange.setBaseArrayLayer(0);
            img_mem_barrier.subresourceRange.setLayerCount(1);
            img_mem_barrier.setSrcAccessMask(src_access_mask);
            img_mem_barrier.setDstAccessMask(dst_access_mask);

            cmd_buf.pipelineBarrier(src_pipeline_stage_flags,
                                    dst_pipeline_stage_flags,
                                    vk::DependencyFlagBits(0),
                                    nullptr,
                                    nullptr,
                                    { img_mem_barrier });
        });
    }

    Texture(const Device *         device,
            const TextureUsageEnum usage_,
            const TextureStateEnum initial_state_,
            const FormatEnum       format,
            const int2             resolution,
            const std::byte *      initial_data        = nullptr,
            StagingBufferManager * initial_data_loader = nullptr,
            const float4           clear_value         = float4(0.0f, 0.0f, 0.0f, 0.0f),
            const std::string &    name                = "")
    : m_clear_value(clear_value)
    {
        assert(resolution.x > 0 && resolution.y > 0);

        vk::ImageUsageFlags usage          = static_cast<vk::ImageUsageFlagBits>(usage_);
        vk::ImageLayout     initial_layout = static_cast<vk::ImageLayout>(initial_state_);
        if (initial_data != nullptr)
        {
            usage |= vk::ImageUsageFlagBits::eTransferDst;
        }

        vk::ImageCreateInfo image_ci_tmp;
        {
            image_ci_tmp.setMipLevels(1);
            image_ci_tmp.setArrayLayers(1);
            image_ci_tmp.setImageType(vk::ImageType::e2D);
            image_ci_tmp.extent.setWidth(resolution.x);
            image_ci_tmp.extent.setHeight(resolution.y);
            image_ci_tmp.extent.setDepth(1);
            image_ci_tmp.setFlags(vk::ImageCreateFlagBits(0));
            image_ci_tmp.setSamples(vk::SampleCountFlagBits::e1);
            image_ci_tmp.setFormat(static_cast<vk::Format>(format));
            image_ci_tmp.setTiling(vk::ImageTiling::eOptimal);
            image_ci_tmp.setInitialLayout(vk::ImageLayout::eUndefined);
            image_ci_tmp.setUsage(usage);
            image_ci_tmp.setSharingMode(vk::SharingMode::eExclusive);
        }
        VkImageCreateInfo image_ci = image_ci_tmp;

        // alloc info
        VmaAllocationCreateInfo vma_alloc_ci = {};
        vma_alloc_ci.usage                   = VmaMemoryUsage::VMA_MEMORY_USAGE_GPU_ONLY;

        // create image
        VkImage           vma_vk_image;
        VmaAllocation     vma_allocation;
        VmaAllocationInfo vma_alloc_info;
        VKCK(vmaCreateImage(*device->m_vma_allocator, &image_ci, &vma_alloc_ci, &vma_vk_image, &vma_allocation, &vma_alloc_info));

        // setup image bundle
        VmaImageBundle image_bundle;
        image_bundle.m_vk_image       = vma_vk_image;
        image_bundle.m_vma_allocation = vma_allocation;
        image_bundle.m_vma_allocator  = device->m_vma_allocator.get();
        image_bundle.m_vma_alloc_info = vma_alloc_info;
        m_vma_image_bundle.m_value    = image_bundle;

        m_vk_image = vma_vk_image;
        device->name_vkhpp_object<vk::Image, vk::Image::CType>(m_vk_image, name);
        m_vk_format     = static_cast<vk::Format>(format);
        m_vk_image_view = create_image_view(device->m_vk_ldevice.get(), m_vk_image, m_vk_format);

        // copy image
        if (initial_data != nullptr)
        {
            const size_t bytes_per_pixel     = EnumHelper::GetSizeInBytesPerPixel(format);
            const size_t image_size_in_bytes = bytes_per_pixel * resolution.x * resolution.y;

            // make image layout optimal for data being transferred to image (TransferDstOptimal)
            transition_image_layout(initial_data_loader->m_vk_command_buffer,
                                    m_vk_image,
                                    vk::ImageLayout::eUndefined,
                                    vk::ImageLayout::eTransferDstOptimal);

            // memory on gpu is not directly mappable, need staging buffer to transfer data
            auto * staging_buffer = initial_data_loader->get_staging_buffer(image_size_in_bytes);
            void * mapped_dst     = staging_buffer->m_vma_alloc_info.pMappedData;
            if (mapped_dst == nullptr)
            {
                VkResult result =
                    vmaMapMemory(staging_buffer->m_vma_allocator, staging_buffer->m_vma_allocation, &mapped_dst);
                assert(result == VK_SUCCESS && mapped_dst != nullptr);
            }
            std::memcpy(mapped_dst, initial_data, image_size_in_bytes);
            vmaUnmapMemory(staging_buffer->m_vma_allocator, staging_buffer->m_vma_allocation);

            // copy
            vk::BufferImageCopy copy_region = {};
            copy_region.setBufferOffset(0);
            copy_region.setBufferImageHeight(resolution.y);
            copy_region.setBufferRowLength(resolution.x);
            copy_region.setImageExtent(vk::Extent3D(resolution.x, resolution.y, 1));
            copy_region.setImageOffset(vk::Offset3D(0, 0, 0));
            copy_region.imageSubresource.setAspectMask(vk::ImageAspectFlagBits::eColor);
            copy_region.imageSubresource.setBaseArrayLayer(0);
            copy_region.imageSubresource.setLayerCount(1);
            copy_region.imageSubresource.setMipLevel(0);
            initial_data_loader->m_vk_command_buffer.copyBufferToImage(staging_buffer->m_vk_buffer,
                                                                       m_vk_image,
                                                                       vk::ImageLayout::eTransferDstOptimal,
                                                                       { copy_region });

            transition_image_layout(initial_data_loader->m_vk_command_buffer,
                                    m_vk_image,
                                    vk::ImageLayout::eTransferDstOptimal,
                                    initial_layout);
        }
        else
        {
            device->one_time_command_submit([&](vk::CommandBuffer cmd_buf) {
                transition_image_layout(cmd_buf, m_vk_image, vk::ImageLayout::eUndefined, initial_layout);
            });
        }

        m_resolution      = int3(resolution, 0);
        m_vk_image_layout = initial_layout;
    }

    bool
    is_empty() const
    {
        return !m_vk_image;
    }

    vk::ImageLayout
    get_attachment_image_layout(const bool readonly = false) const
    {
        // swapchain?
        if (!m_vma_image_bundle.empty()) return vk::ImageLayout::ePresentSrcKHR;

        // depth only
        if (m_vk_format == vk::Format::eD16Unorm || m_vk_format == vk::Format::eD32Sfloat)
            return readonly ? vk::ImageLayout::eDepthReadOnlyOptimal : vk::ImageLayout::eDepthAttachmentOptimal;

        // stencil only
        if (m_vk_format == vk::Format::eS8Uint)
            return readonly ? vk::ImageLayout::eStencilReadOnlyOptimal : vk::ImageLayout::eStencilAttachmentOptimal;

        // depth stencil
        if (m_vk_format == vk::Format::eD16UnormS8Uint ||
            m_vk_format == vk::Format::eD24UnormS8Uint || m_vk_format == vk::Format::eD32SfloatS8Uint)
            return readonly ? vk::ImageLayout::eDepthAttachmentStencilReadOnlyOptimal
                            : vk::ImageLayout::eDepthAttachmentOptimal;

        assert(readonly == false);

        // undefined
        if (m_vk_format == vk::Format::eUndefined) return vk::ImageLayout::eUndefined;

        return vk::ImageLayout::eColorAttachmentOptimal;
    }

    vk::ImageLayout
    get_optimal_image_layout(const vk::ImageUsageFlags image_usage_bit)
    {
        if (image_usage_bit & vk::ImageUsageFlagBits::eColorAttachment)
        {
            return vk::ImageLayout::eColorAttachmentOptimal;
        }
        else if (image_usage_bit & vk::ImageUsageFlagBits::eDepthStencilAttachment)
        {
            return vk::ImageLayout::eDepthStencilAttachmentOptimal;
        }
        else if (image_usage_bit & vk::ImageUsageFlagBits::eSampled)
        {
            return vk::ImageLayout::eShaderReadOnlyOptimal;
        }
        else if (image_usage_bit & vk::ImageUsageFlagBits::eStorage)
        {
            return vk::ImageLayout::eGeneral;
        }
        else if (image_usage_bit & vk::ImageUsageFlagBits::eTransferDst)
        {
            return vk::ImageLayout::eTransferDstOptimal;
        }
        else if (image_usage_bit & vk::ImageUsageFlagBits::eTransferSrc)
        {
            return vk::ImageLayout::eTransferSrcOptimal;
        }
        return vk::ImageLayout::eUndefined;
    }

    vk::UniqueImageView
    create_image_view(const vk::Device device, const vk::Image image, const vk::Format format)
    {
        vk::ImageViewCreateInfo image_view_ci;
        image_view_ci.setImage(image);
        image_view_ci.setViewType(vk::ImageViewType::e2D);
        image_view_ci.setFormat(format);
        image_view_ci.components.setR(vk::ComponentSwizzle::eIdentity);
        image_view_ci.components.setG(vk::ComponentSwizzle::eIdentity);
        image_view_ci.components.setB(vk::ComponentSwizzle::eIdentity);
        image_view_ci.components.setA(vk::ComponentSwizzle::eIdentity);
        image_view_ci.subresourceRange.setAspectMask(vk::ImageAspectFlagBits::eColor);
        image_view_ci.subresourceRange.setBaseMipLevel(0);
        image_view_ci.subresourceRange.setLevelCount(1);
        image_view_ci.subresourceRange.setBaseArrayLayer(0);
        image_view_ci.subresourceRange.setLayerCount(1);
        return device.createImageViewUnique(image_view_ci);
    }

    vk::PipelineStageFlags
    pipeline_stage_flags(const vk::ImageLayout vk_image_layout)
    {
        switch (vk_image_layout)
        {
        case vk::ImageLayout::eTransferDstOptimal:
        case vk::ImageLayout::eTransferSrcOptimal:
            return vk::PipelineStageFlagBits::eTransfer;
        case vk::ImageLayout::eColorAttachmentOptimal:
            return vk::PipelineStageFlagBits::eColorAttachmentOutput;
        case vk::ImageLayout::eDepthStencilAttachmentOptimal:
            return vk::PipelineStageFlagBits::eEarlyFragmentTests;
        case vk::ImageLayout::eShaderReadOnlyOptimal:
            return vk::PipelineStageFlagBits::eFragmentShader;
        case vk::ImageLayout::ePreinitialized:
            return vk::PipelineStageFlagBits::eHost;
        case vk::ImageLayout::eUndefined:
            return vk::PipelineStageFlagBits::eTopOfPipe;
        default:
            return vk::PipelineStageFlagBits::eBottomOfPipe;
        }
    }

    vk::AccessFlags
    access_flags_for_layout(vk::ImageLayout layout)
    {
        switch (layout)
        {
        case vk::ImageLayout::ePreinitialized:
            return vk::AccessFlagBits::eHostWrite;
        case vk::ImageLayout::eTransferDstOptimal:
            return vk::AccessFlagBits::eTransferWrite;
        case vk::ImageLayout::eTransferSrcOptimal:
            return vk::AccessFlagBits::eTransferRead;
        case vk::ImageLayout::eColorAttachmentOptimal:
            return vk::AccessFlagBits::eColorAttachmentWrite;
        case vk::ImageLayout::eDepthStencilAttachmentOptimal:
            return vk::AccessFlagBits::eDepthStencilAttachmentWrite;
        case vk::ImageLayout::eShaderReadOnlyOptimal:
            return vk::AccessFlagBits::eShaderRead;
        default:
            return vk::AccessFlags();
        }
    }

    void
    transition_image_layout(vk::CommandBuffer       command_buffer,
                            const vk::Image &       vk_image,
                            const vk::ImageLayout & old_vk_image_layout,
                            const vk::ImageLayout & new_vk_image_layout)
    {
        vk::AccessFlags        src_access_mask = access_flags_for_layout(old_vk_image_layout);
        vk::AccessFlags        dst_access_mask = access_flags_for_layout(new_vk_image_layout);
        vk::PipelineStageFlags src_pipeline_stage_flags = pipeline_stage_flags(old_vk_image_layout);
        vk::PipelineStageFlags dst_pipeline_stage_flags = pipeline_stage_flags(new_vk_image_layout);

        // setup barrier make sure that image is properly setup
        vk::ImageMemoryBarrier img_mem_barrier;
        {
            img_mem_barrier.setOldLayout(old_vk_image_layout);
            img_mem_barrier.setNewLayout(new_vk_image_layout);
            img_mem_barrier.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
            img_mem_barrier.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
            img_mem_barrier.setImage(vk_image);
            img_mem_barrier.subresourceRange.setAspectMask(vk::ImageAspectFlagBits::eColor);
            img_mem_barrier.subresourceRange.setBaseMipLevel(0);
            img_mem_barrier.subresourceRange.setLevelCount(1);
            img_mem_barrier.subresourceRange.setBaseArrayLayer(0);
            img_mem_barrier.subresourceRange.setLayerCount(1);
            img_mem_barrier.setSrcAccessMask(src_access_mask);
            img_mem_barrier.setDstAccessMask(dst_access_mask);
        }

        command_buffer.pipelineBarrier(src_pipeline_stage_flags,
                                       dst_pipeline_stage_flags,
                                       vk::DependencyFlagBits(0),
                                       nullptr,
                                       nullptr,
                                       { img_mem_barrier });
    }
};
} // namespace VKA_NAME
#endif