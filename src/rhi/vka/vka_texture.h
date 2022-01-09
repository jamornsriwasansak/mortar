#pragma once

#include "pch/pch.h"

#ifdef USE_VKA

    #include "vka_aliasable_memory.h"
    #include "vka_common.h"
    #include "vka_constants.h"
    #include "vka_device.h"
    #include "vka_stagingbuffermanager.h"
    #include "vka_swapchain.h"

namespace VKA_NAME
{
struct Texture
{
    struct VmaImageBundle
    {
        VmaAllocator      m_vma_allocator  = nullptr;
        VmaAllocation     m_vma_allocation = nullptr;
        VmaAllocationInfo m_vma_alloc_info;
        vk::Image         m_vk_image;
    };

    struct VmaImageBundleDeleter
    {
        VmaImageBundleDeleter() {}
        void
        operator()(VmaImageBundle bundle)
        {
            if (bundle.m_vma_allocator)
            {
                // Destroy using vma allocator
                // vk image and its memory is allocated altogether in the constructor
                vmaDestroyImage(bundle.m_vma_allocator,
                                static_cast<VkImage>(bundle.m_vk_image),
                                bundle.m_vma_allocation);
            }
        }
    };

    using UniqueVmaBundle = UniqueVarHandle<VmaImageBundle, VmaImageBundleDeleter>;

    int3           m_resolution;
    const Device & m_device;
    // Stores three different type of data depends on how we construct it
    std::variant<UniqueVmaBundle, vk::Image, vk::UniqueImage> m_image_variant;
    vk::UniqueImageView                                       m_vk_image_view;
    vk::Format                                                m_vk_format;

    Texture(const std::string & name, const Device & device, const Swapchain & swapchain, const size_t i_image)
    : m_device(device)
    {
        m_image_variant = device.m_vk_ldevice->getSwapchainImagesKHR(*swapchain.m_vk_swapchain)[i_image];
        m_vk_image_view = create_image_view(device.m_vk_ldevice.get(),
                                            std::get<vk::Image>(m_image_variant),
                                            swapchain.m_vk_format);
        m_vk_format     = swapchain.m_vk_format;
        m_resolution    = int3(swapchain.m_resolution, 1);

        // convert the layout into present khr
        device.one_time_command_submit(
            [&](vk::CommandBuffer cmd_buf)
            {
                vk::ImageLayout old_vk_image_layout = vk::ImageLayout::eUndefined;
                vk::ImageLayout new_vk_image_layout = vk::ImageLayout::ePresentSrcKHR;

                vk::AccessFlags src_access_mask = access_flags_for_layout(old_vk_image_layout);
                vk::AccessFlags dst_access_mask = access_flags_for_layout(new_vk_image_layout);
                vk::PipelineStageFlags src_pipeline_stage_flags = pipeline_stage_flags(old_vk_image_layout);
                vk::PipelineStageFlags dst_pipeline_stage_flags = pipeline_stage_flags(new_vk_image_layout);

                // setup barrier make sure that image is properly setup
                vk::ImageMemoryBarrier img_mem_barrier;
                img_mem_barrier.setOldLayout(old_vk_image_layout);
                img_mem_barrier.setNewLayout(new_vk_image_layout);
                img_mem_barrier.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
                img_mem_barrier.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
                img_mem_barrier.setImage(std::get<vk::Image>(m_image_variant));
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
        if (!name.empty())
        {
            device.name_vkhpp_object(std::get<vk::Image>(m_image_variant), name);
            device.name_vkhpp_object(*m_vk_image_view, name + "_image_view");
        }
    }

    Texture(const std::string &            name,
            const Device &                 device,
            const Rhi::TextureCreateInfo & create_info,
            const Rhi::TextureStateEnum &  stage_enum)
    : m_device(device)
    {
        // Setup image create info
        VkImageCreateInfo image_ci = GetVkImageCreateInfo(create_info);

        // Setup alloc info
        VmaAllocationCreateInfo vma_alloc_ci = {};
        vma_alloc_ci.usage                   = VmaMemoryUsage::VMA_MEMORY_USAGE_GPU_ONLY;

        // Create image using VMA
        VmaImageBundle vma_bundle;
        VkImage        _vk_image;
        vma_bundle.m_vma_allocator = device.m_vma_allocator.get();
        VKCK(vmaCreateImage(vma_bundle.m_vma_allocator,
                            &image_ci,
                            &vma_alloc_ci,
                            &_vk_image,
                            &vma_bundle.m_vma_allocation,
                            &vma_bundle.m_vma_alloc_info));
        vma_bundle.m_vk_image = _vk_image;

        // Set values
        m_image_variant = vma_bundle;
        m_vk_format     = GetVkFormat(create_info.m_format);
        m_vk_image_view = create_image_view(device.m_vk_ldevice.get(), _vk_image, m_vk_format);
        m_resolution    = int3(create_info.m_width, create_info.m_height, create_info.m_depth);

        // Transition to target image layout
        device.one_time_command_submit(
            [&](vk::CommandBuffer cmd_buf)
            {
                transition_image_layout(cmd_buf, _vk_image, vk::ImageLayout::eUndefined, GetVkImageLayout(stage_enum));
            });

        // Set names
        if (!name.empty())
        {
            device.name_vkhpp_object(vk::Image(_vk_image), name);
            device.name_vkhpp_object(*m_vk_image_view, name + "_image_view");
        }
    }

    Texture(const std::string &            name,
            const Device &                 device,
            const Rhi::TextureCreateInfo & create_info,
            const Rhi::TextureStateEnum &  stage_enum,
            const Rhi::AliasableMemory &   memory,
            const size_t                   offset_into_memory_in_bytes)
    : m_device(device)
    {
        // Setup image create info
        VkImageCreateInfo image_ci = GetVkImageCreateInfo(create_info);

        // Create image
        m_image_variant = device.m_vk_ldevice->createImage(image_ci);

        // Bind memory
        vmaBindImageMemory2(memory.m_vma_memory_bundle->m_vma_allocator,
                            memory.m_vma_memory_bundle->m_vma_allocation,
                            offset_into_memory_in_bytes,
                            std::get<vk::UniqueImage>(m_image_variant).get(),
                            nullptr);

        // Set values
        m_vk_format     = GetVkFormat(create_info.m_format);
        m_vk_image_view = create_image_view(device.m_vk_ldevice.get(),
                                            std::get<vk::UniqueImage>(m_image_variant).get(),
                                            m_vk_format);
        m_resolution    = int3(create_info.m_width, create_info.m_height, create_info.m_depth);

        // Transition to target image layout
        device.one_time_command_submit(
            [&](vk::CommandBuffer cmd_buf)
            {
                transition_image_layout(cmd_buf,
                                        std::get<vk::UniqueImage>(m_image_variant).get(),
                                        vk::ImageLayout::eUndefined,
                                        GetVkImageLayout(stage_enum));
            });

        // Set names
        if (!name.empty())
        {
            device.name_vkhpp_object(std::get<vk::UniqueImage>(m_image_variant).get(), name);
            device.name_vkhpp_object(*m_vk_image_view, name + "_image_view");
        }
    }

    static vk::ImageCreateInfo
    GetVkImageCreateInfo(const Rhi::TextureCreateInfo & create_info)
    {
        // Assign create info
        vk::ImageCreateInfo image_ci;
        image_ci.setImageType(vk::ImageType::e2D);
        image_ci.extent.setWidth(create_info.m_width);
        image_ci.extent.setHeight(create_info.m_height);
        image_ci.extent.setDepth(create_info.m_depth);
        image_ci.setMipLevels(create_info.m_mip_levels);
        image_ci.setArrayLayers(1);
        image_ci.setFormat(GetVkFormat(create_info.m_format));
        image_ci.setTiling(vk::ImageTiling::eOptimal);
        image_ci.setInitialLayout(vk::ImageLayout::eUndefined);
        image_ci.setUsage(GetVkUsageFlags(create_info.m_texture_usage) | vk::ImageUsageFlagBits::eSampled);
        image_ci.setSamples(vk::SampleCountFlagBits::e1);
        image_ci.setSharingMode(vk::SharingMode::eExclusive);
        return image_ci;
    }

    vk::Image
    get_vk_image() const
    {
        if (m_image_variant.index() == 0)
        {
            return std::get<0>(m_image_variant)->m_vk_image;
        }
        else if (m_image_variant.index() == 1)
        {
            return std::get<1>(m_image_variant);
        }
        else // if (m_image_variant.index() == 2)
        {
            return std::get<2>(m_image_variant).get();
        }
    }

    vk::ImageLayout
    get_attachment_image_layout(const bool readonly = false) const
    {
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