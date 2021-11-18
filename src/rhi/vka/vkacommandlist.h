#pragma once

#include "vkacommon.h"

#ifdef USE_VKA

#include "rhi/commontypes/rhicopyregion.h"
#include "vkadescriptor.h"
#include "vkadevice.h"
#include "vkafence.h"
#include "vkaframebufferbinding.h"
#include "vkaimguirenderpass.h"
#include "vkarasterpipeline.h"
#include "vkaraytracingpipeline.h"
#include "vkasemaphore.h"
#include "vkatexture.h"

namespace VKA_NAME
{

struct BufferCopyInfo
{
    vk::BufferCopy vk_copy_region;

    BufferCopyInfo() {}

    void
    set_size(const size_t size)
    {
        vk_copy_region.setSize(size);
    }

    void
    set_src_offset(const size_t src_offset)
    {
        vk_copy_region.setSrcOffset(src_offset);
    }

    void
    set_dst_offset(const size_t dst_offset)
    {
        vk_copy_region.setDstOffset(dst_offset);
    }
};
static_assert(sizeof(vk::BufferCopy) == sizeof(BufferCopyInfo),
              "sizeof(vk::BufferCopy) == sizeof(BufferCopyInfo)");

struct CommandList
{
    vk::Queue         m_vk_queue          = {};
    vk::CommandBuffer m_vk_command_buffer = {};

    CommandList() {}

    CommandList(const vk::Queue queue, const vk::CommandBuffer command_buffer)
    : m_vk_queue(queue), m_vk_command_buffer(command_buffer)
    {
    }

    void
    begin() const
    {
        vk::CommandBufferBeginInfo begin_info;
        begin_info.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
        m_vk_command_buffer.begin(begin_info);
    }

    void
    begin_render_pass(const FramebufferBindings & framebuffer_binding)
    {
        vk::RenderPassBeginInfo render_pass_bi;
        render_pass_bi.setRenderPass(framebuffer_binding.m_vk_render_pass.get());
        render_pass_bi.setFramebuffer(framebuffer_binding.m_vk_framebuffer.get());
        render_pass_bi.renderArea.offset.x = 0;
        render_pass_bi.renderArea.offset.y = 0;
        render_pass_bi.renderArea.extent =
            vk::Extent2D(framebuffer_binding.m_resolution.x, framebuffer_binding.m_resolution.y);
        m_vk_command_buffer.beginRenderPass(render_pass_bi, vk::SubpassContents::eInline);
    }

    void
    bind_raytrace_pipeline(const RayTracingPipeline & rt_pipeline)
    {
        m_vk_command_buffer.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR,
                                         rt_pipeline.m_vk_pipeline.get());
    }

    void
    bind_raster_pipeline(const RasterPipeline & raster_pipeline)
    {
        m_vk_command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                         raster_pipeline.m_vk_pipeline.get());
    }

    void
    bind_descriptor_set(const DescriptorSet * desc_sets, const size_t num_desc_sets, vk::PipelineBindPoint pipeline_bind_point)
    {
        std::vector<vk::DescriptorSet> vk_desc_sets(num_desc_sets);
        for (size_t i = 0; i < num_desc_sets; i++)
        {
            vk_desc_sets[i] = desc_sets[i].m_vk_descriptor_set;
        }
        m_vk_command_buffer.bindDescriptorSets(pipeline_bind_point,
                                               desc_sets[0].m_vk_pipeline_layout,
                                               0u,
                                               static_cast<uint32_t>(vk_desc_sets.size()),
                                               vk_desc_sets.data(),
                                               0u,
                                               nullptr);
    }

    void
    bind_compute_descriptor_set(const std::span<DescriptorSet> & desc_sets)
    {
        bind_descriptor_set(desc_sets.data(), desc_sets.size(), vk::PipelineBindPoint::eCompute);
    }

    void
    bind_graphics_descriptor_set(const std::span<DescriptorSet> & desc_sets)
    {
        bind_descriptor_set(desc_sets.data(), desc_sets.size(), vk::PipelineBindPoint::eGraphics);
    }

    void
    bind_raytrace_descriptor_set(const std::span<DescriptorSet> & desc_sets)
    {
        bind_descriptor_set(desc_sets.data(), desc_sets.size(), vk::PipelineBindPoint::eRayTracingKHR);
    }

    void
    bind_vertex_buffer(const Buffer & vertex_buffer, [[maybe_unused]] const size_t stride)
    {
        // stride information has already been used in vk::VertexInputBindingDescription
        m_vk_command_buffer.bindVertexBuffers(0, { vertex_buffer.m_vma_buffer_bundle->m_vk_buffer }, { 0 });
    }

    void
    bind_index_buffer(const Buffer & index_buffer, const IndexType index_type)
    {
        m_vk_command_buffer.bindIndexBuffer(index_buffer.m_vma_buffer_bundle->m_vk_buffer,
                                            0,
                                            static_cast<vk::IndexType>(index_type));
    }

    void
    copy_buffer_region(const Buffer & dst_buffer,
                       const size_t   dst_offset_in_bytes,
                       const Buffer & src_buffer,
                       const size_t   src_offset_in_bytes,
                       const size_t   size_in_bytes)
    {
        vk::BufferCopy copy_region = {};
        copy_region.setSize(size_in_bytes);
        copy_region.setSrcOffset(src_offset_in_bytes);
        copy_region.setDstOffset(dst_offset_in_bytes);
        m_vk_command_buffer.copyBuffer(src_buffer.m_vma_buffer_bundle->m_vk_buffer,
                                       dst_buffer.m_vma_buffer_bundle->m_vk_buffer,
                                       { copy_region });
    }

    void
    draw_instanced(const uint32_t vertex_count_per_instance,
                   const uint32_t instance_count,
                   const uint32_t first_vertex,
                   const uint32_t first_instance)
    {
        m_vk_command_buffer.draw(vertex_count_per_instance, instance_count, first_vertex, first_instance);
    }

    void
    end_render_pass()
    {
        m_vk_command_buffer.endRenderPass();
    }

    void
    end() const
    {
        m_vk_command_buffer.end();
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
    render_imgui(const ImGuiRenderPass & imgui_render_pass, const size_t i_image)
    {
        ImGui::Render();

        vk::RenderPassBeginInfo render_pass_bi;
        render_pass_bi.setRenderPass(imgui_render_pass.m_vk_render_pass.get());
        render_pass_bi.setFramebuffer(imgui_render_pass.m_vk_framebuffers[i_image].get());
        render_pass_bi.renderArea.offset.x = 0;
        render_pass_bi.renderArea.offset.y = 0;
        render_pass_bi.renderArea.extent =
            vk::Extent2D(imgui_render_pass.m_resolution.x, imgui_render_pass.m_resolution.y);
        m_vk_command_buffer.beginRenderPass(render_pass_bi, vk::SubpassContents::eInline);
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), m_vk_command_buffer);
        m_vk_command_buffer.endRenderPass();
    }

    void
    submit(Fence * fence, const Semaphore * semaphore_to_wait = nullptr, const Semaphore * semaphore_to_signal = nullptr)
    {
        vk::SubmitInfo         submit_info    = {};
        vk::PipelineStageFlags dst_stage_mask = vk::PipelineStageFlagBits::eBottomOfPipe;
        if (semaphore_to_wait != nullptr)
        {
            submit_info.setPWaitSemaphores(&semaphore_to_wait->m_vk_semaphore.get());
            submit_info.setWaitSemaphoreCount(1u);
            submit_info.setPWaitDstStageMask(&dst_stage_mask);
        }
        if (semaphore_to_signal != nullptr)
        {
            submit_info.setPSignalSemaphores(&semaphore_to_signal->m_vk_semaphore.get());
            submit_info.setSignalSemaphoreCount(1u);
        }
        submit_info.setPCommandBuffers(&m_vk_command_buffer);
        submit_info.setCommandBufferCount(1);

        if (fence)
        {
            m_vk_queue.submit({ submit_info }, fence->m_vk_fence.get());
        }
        else
        {
            m_vk_queue.submit({ submit_info });
        }
    }

    void
    transition_texture(const Texture & texture, const TextureStateEnum pre_enum, const TextureStateEnum post_enum)
    {
        vk::ImageLayout old_vk_image_layout = static_cast<vk::ImageLayout>(pre_enum);
        vk::ImageLayout new_vk_image_layout = static_cast<vk::ImageLayout>(post_enum);

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
        img_mem_barrier.setImage(texture.m_vk_image);
        img_mem_barrier.subresourceRange.setAspectMask(vk::ImageAspectFlagBits::eColor);
        img_mem_barrier.subresourceRange.setBaseMipLevel(0);
        img_mem_barrier.subresourceRange.setLevelCount(1);
        img_mem_barrier.subresourceRange.setBaseArrayLayer(0);
        img_mem_barrier.subresourceRange.setLayerCount(1);
        img_mem_barrier.setSrcAccessMask(src_access_mask);
        img_mem_barrier.setDstAccessMask(dst_access_mask);

        m_vk_command_buffer.pipelineBarrier(src_pipeline_stage_flags,
                                            dst_pipeline_stage_flags,
                                            vk::DependencyFlagBits(0),
                                            nullptr,
                                            nullptr,
                                            { img_mem_barrier });
    }

    void
    trace_rays(const RayTracingShaderTable & table,
               const uint32_t                width  = 1,
               const uint32_t                height = 1,
               const uint32_t                depth  = 1)
    {
        m_vk_command_buffer.traceRaysKHR(table.m_raygen_device_region,
                                         table.m_miss_device_region,
                                         table.m_hitgroup_device_region,
                                         table.m_callable_device_region,
                                         width,
                                         height,
                                         depth);
    }
};
} // namespace VKA_NAME
#endif