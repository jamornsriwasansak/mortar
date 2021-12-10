#pragma once

#include "pch/pch.h"

#ifdef USE_DXA

    #include "dxa_buffer.h"
    #include "dxa_common.h"
    #include "dxa_descriptor.h"
    #include "dxa_entry.h"
    #include "dxa_fence.h"
    #include "dxa_framebufferbinding.h"
    #include "dxa_imguirenderpass.h"
    #include "dxa_rasterpipeline.h"
    #include "dxa_raytracingpipeline.h"
    #include "dxa_semaphore.h"
    #include "dxa_swapchain.h"
    #include "dxa_texture.h"

namespace DXA_NAME
{
struct CommandList
{
    ComPtr<ID3D12GraphicsCommandList4> m_dx_cmd_list  = nullptr;
    ID3D12CommandQueue *               m_dx_cmd_queue = nullptr;

    // TODO:: avoid std::list
    std::list<std::vector<D3D12_VERTEX_BUFFER_VIEW>> m_bound_vertex_buffer_views;
    ID3D12RootSignature *                            m_dx_root_signature = nullptr;

    CommandList() {}

    CommandList(ID3D12CommandQueue * dx_cmd_queue, ComPtr<ID3D12GraphicsCommandList4> & dx_cmd_list)
    : m_dx_cmd_queue(dx_cmd_queue), m_dx_cmd_list(dx_cmd_list)
    {
    }

    void
    begin()
    {
    }

    void
    bind_descriptor_set(const DescriptorSet * desc_sets, const size_t num_desc_sets, bool is_graphics)
    {
        assert(desc_sets->m_updated);

        ID3D12DescriptorHeap * heaps[] = {
            desc_sets->m_descriptor_pool->m_cbv_srv_uav_heap.m_dx_descriptor_heap.Get(),
            desc_sets->m_descriptor_pool->m_sampler_heap.m_dx_descriptor_heap.Get()
        };
        m_dx_cmd_list->SetDescriptorHeaps(_countof(heaps), heaps);

        for (size_t i_set = 0; i_set < num_desc_sets; i_set++)
        {
            assert(desc_sets[0].m_descriptor_pool == desc_sets[i_set].m_descriptor_pool);
            assert(desc_sets[0].m_descriptor_pool == desc_sets[i_set].m_descriptor_pool);

            const DescriptorSet & desc_set = desc_sets[i_set];

            // set root signature level crv
            for (size_t i = 0; i < desc_set.m_root_cbvs.size(); i++)
            {
                if (is_graphics)
                {
                    m_dx_cmd_list->SetGraphicsRootConstantBufferView(
                        static_cast<UINT>(desc_set.m_root_cbvs[i].m_root_signature_index),
                        desc_set.m_root_cbvs[i].m_virtual_address);
                }
                else
                {
                    m_dx_cmd_list->SetComputeRootConstantBufferView(
                        static_cast<UINT>(desc_set.m_root_cbvs[i].m_root_signature_index),
                        desc_set.m_root_cbvs[i].m_virtual_address);
                }
            }

            // set root signature level srv
            for (size_t i = 0; i < desc_set.m_root_srvs.size(); i++)
            {
                if (is_graphics)
                {
                    m_dx_cmd_list->SetGraphicsRootConstantBufferView(
                        static_cast<UINT>(desc_set.m_root_srvs[i].m_root_signature_index),
                        desc_set.m_root_srvs[i].m_virtual_address);
                }
                else
                {
                    m_dx_cmd_list->SetComputeRootShaderResourceView(
                        static_cast<UINT>(desc_set.m_root_srvs[i].m_root_signature_index),
                        desc_set.m_root_srvs[i].m_virtual_address);
                }
            }

            // set root signature level descriptor table
            for (size_t i = 0; i < desc_set.m_root_descriptor_tables.size(); i++)
            {
                if (is_graphics)
                {
                    m_dx_cmd_list->SetGraphicsRootDescriptorTable(
                        static_cast<UINT>(desc_set.m_root_descriptor_tables[i].m_root_signature_index),
                        desc_set.m_root_descriptor_tables[i].m_gpu_handle);
                }
                else
                {
                    m_dx_cmd_list->SetComputeRootDescriptorTable(
                        static_cast<UINT>(desc_set.m_root_descriptor_tables[i].m_root_signature_index),
                        desc_set.m_root_descriptor_tables[i].m_gpu_handle);
                }
            }
        }
    }

    void
    bind_graphics_descriptor_set(const std::span<DescriptorSet> & desc_sets)
    {
        bind_descriptor_set(desc_sets.data(), desc_sets.size(), true);
    }

    void
    bind_compute_descriptor_set(const std::span<DescriptorSet> & desc_sets)
    {
        bind_descriptor_set(desc_sets.data(), desc_sets.size(), false);
    }

    void
    bind_ray_trace_descriptor_set(const std::span<DescriptorSet> & desc_sets)
    {
        bind_descriptor_set(desc_sets.data(), desc_sets.size(), false);
    }

    void
    bind_index_buffer(const Buffer &  buffer,
                      const IndexType index_type,
                      const size_t    size_in_bytes   = std::numeric_limits<size_t>::max(),
                      const size_t    offset_in_bytes = 0)
    {
        D3D12_INDEX_BUFFER_VIEW index_buffer_view;
        index_buffer_view.BufferLocation =
            buffer.m_allocation->GetResource()->GetGPUVirtualAddress() + offset_in_bytes;
        index_buffer_view.SizeInBytes = size_in_bytes == std::numeric_limits<size_t>::max()
                                            ? static_cast<UINT>(buffer.m_size_in_bytes)
                                            : static_cast<UINT>(size_in_bytes);
        index_buffer_view.Format      = static_cast<DXGI_FORMAT>(index_type);

        m_dx_cmd_list->IASetIndexBuffer(&index_buffer_view);
    }

    void
    begin_render_pass(const FramebufferBindings & framebuffer_binding)
    {
        constexpr size_t NumMaxHandles = 4;
        assert(framebuffer_binding.m_color_attachments.size() < NumMaxHandles);

        // set rtv handles
        D3D12_CPU_DESCRIPTOR_HANDLE rtv_handles[NumMaxHandles];
        size_t num_rtv_handles = static_cast<UINT>(framebuffer_binding.m_color_attachments.size());
        for (size_t i_handle = 0; i_handle < framebuffer_binding.m_color_attachments.size(); i_handle++)
        {
            rtv_handles[i_handle] = framebuffer_binding.m_color_attachments[i_handle]->m_dx_dsv_rtv_cpu_handle;
        }

        // set dsv handle
        const D3D12_CPU_DESCRIPTOR_HANDLE * dsv_handle = nullptr;
        if (framebuffer_binding.m_depth_attachment)
        {
            dsv_handle = &framebuffer_binding.m_depth_attachment->m_dx_dsv_rtv_cpu_handle;
        }

        // set render targets
        m_dx_cmd_list->OMSetRenderTargets(static_cast<UINT>(num_rtv_handles), rtv_handles, FALSE, dsv_handle);
    }

    /*
    void
    bind_material_system(const MaterialSystem & mat_sys)
    {
        ID3D12DescriptorHeap * heaps[] = { Dev(0)->m_dx_cbv_srv_uav_descriptor_heap.Get(),
                                           Dev(0)->m_dx_sampler_heap.Get() };
        m_dx_cmd_list->SetDescriptorHeaps(_countof(heaps), heaps);

        // set sample handle
        m_dx_cmd_list->SetGraphicsRootDescriptorTable(2u, mat_sys.m_texture_gpu_handle);

        // set sampler
        m_dx_cmd_list->SetGraphicsRootDescriptorTable(3u, mat_sys.m_sampler_gpu_handle);
    }

    void
    bind_material(const Buffer & mat_buf)
    {
        // bind root signature and pipeline state
        m_dx_cmd_list->SetGraphicsRootConstantBufferView(1u, mat_buf.m_allocation->GetResource()->GetGPUVirtualAddress());
    }
    */

    void
    bind_vertex_buffer(const Buffer & buffer,
                       const size_t   stride_size_in_bytes,
                       const size_t   size_in_bytes = std::numeric_limits<size_t>::max(),
                       const size_t   offset        = 0)
    {
        // reserve vertex buffer view and resize it
        m_bound_vertex_buffer_views.resize(m_bound_vertex_buffer_views.size() + 1);
        D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view;
        vertex_buffer_view.BufferLocation = buffer.m_allocation->GetResource()->GetGPUVirtualAddress() + offset;
        vertex_buffer_view.StrideInBytes = static_cast<UINT>(stride_size_in_bytes);
        vertex_buffer_view.SizeInBytes   = static_cast<UINT>(
            size_in_bytes == std::numeric_limits<size_t>::max() ? buffer.m_size_in_bytes : size_in_bytes);

        // finally set vertexbuffer
        m_dx_cmd_list->IASetVertexBuffers(0, 1, &vertex_buffer_view);
    }

    void
    bind_ray_trace_pipeline(const RayTracingPipeline & pipeline)
    {
        m_dx_cmd_list->SetComputeRootSignature(pipeline.m_dx_global_root_signature.Get());
        m_dx_cmd_list->SetPipelineState1(pipeline.m_dx_rt_pso.Get());
    }

    void
    bind_raster_pipeline(const RasterPipeline & pipeline)
    {
        m_dx_cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_dx_cmd_list->SetGraphicsRootSignature(pipeline.m_dx_root_signature.Get());
        // set pipeline state, viewport and scissor.
        // viewport and scissor are simulataneously set to emulate vulkan constraint
        m_dx_cmd_list->SetPipelineState(pipeline.m_dx_pso.Get());
        m_dx_cmd_list->RSSetViewports(1, &pipeline.m_dx_viewport);
        m_dx_cmd_list->RSSetScissorRects(1, &pipeline.m_dx_scissor_rect);
    }

    void
    bind_ray_tracing_pipeline(const RayTracingPipeline & pipeline)
    {
        m_dx_cmd_list->SetComputeRootSignature(pipeline.m_dx_global_root_signature.Get());
        m_dx_cmd_list->SetPipelineState1(pipeline.m_dx_rt_pso.Get());
    }

    void
    copy_buffer_region(const Buffer & dst_buffer,
                       const size_t   dst_offset_in_bytes,
                       const Buffer & src_buffer,
                       const size_t   src_offset_in_bytes,
                       const size_t   size_in_bytes)
    {
        m_dx_cmd_list->CopyBufferRegion(dst_buffer.m_allocation->GetResource(),
                                        dst_offset_in_bytes,
                                        src_buffer.m_allocation->GetResource(),
                                        src_offset_in_bytes,
                                        size_in_bytes);
    }

    void
    end_render_pass()
    {
    }

    void
    copy_texture(const Texture & src, const Texture & dst)
    {
        m_dx_cmd_list->CopyResource(src.m_dx_resource, dst.m_dx_resource);
    }

    void
    clear_color_render_target(const Texture & render_target)
    {
        FLOAT clear_color[4];
        clear_color[0] = render_target.m_clear_value[0];
        clear_color[1] = render_target.m_clear_value[1];
        clear_color[2] = render_target.m_clear_value[2];
        clear_color[3] = render_target.m_clear_value[3];
        m_dx_cmd_list->ClearRenderTargetView(render_target.m_dx_dsv_rtv_cpu_handle, clear_color, 0, nullptr);
    }

    void
    clear_depth_render_target(const Texture & render_target)
    {
        m_dx_cmd_list->ClearDepthStencilView(render_target.m_dx_dsv_rtv_cpu_handle,
                                             D3D12_CLEAR_FLAG_DEPTH,
                                             render_target.m_clear_value[0],
                                             0,
                                             0,
                                             nullptr);
    }

    void
    draw_instanced(const UINT vertex_count_per_instance, const UINT instance_count, const UINT first_vertex, const UINT first_instance)
    {
        m_dx_cmd_list->DrawInstanced(vertex_count_per_instance, instance_count, first_vertex, first_instance);
    }

    void
    draw_indexed_instanced(const size_t index_count_per_instance,
                           const size_t instance_count          = 1,
                           const size_t start_index_location    = 0,
                           const size_t base_vertex_location    = 0,
                           const size_t start_instance_location = 0)
    {
        m_dx_cmd_list->DrawIndexedInstanced(static_cast<UINT>(index_count_per_instance),
                                            static_cast<UINT>(instance_count),
                                            static_cast<UINT>(start_index_location),
                                            static_cast<INT>(base_vertex_location),
                                            static_cast<UINT>(start_instance_location));
    }

    void
    end()
    {
        DXCK(m_dx_cmd_list->Close());
        m_dx_root_signature = nullptr;
    }

    void
    prepare_swapchain_as_render_target(const Swapchain & swapchain)
    {
        CD3DX12_RESOURCE_BARRIER barrier_present_to_rtv = CD3DX12_RESOURCE_BARRIER::Transition(
            swapchain.m_dx_swapchain_resource_pointers[swapchain.m_image_index].Get(),
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
        m_dx_cmd_list->ResourceBarrier(1, &barrier_present_to_rtv);
    }

    void
    prepare_swapchain_for_present(const Swapchain & swapchain)
    {
        CD3DX12_RESOURCE_BARRIER barrier_rtv_to_present = CD3DX12_RESOURCE_BARRIER::Transition(
            swapchain.m_dx_swapchain_resource_pointers[swapchain.m_image_index].Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PRESENT);
        m_dx_cmd_list->ResourceBarrier(1, &barrier_rtv_to_present);
    }

    void
    render_imgui(const ImGuiRenderPass & imgui_render_pass, [[maybe_unused]] const size_t i_image)
    {
        ImGui::Render();
        ID3D12DescriptorHeap * p[] = { imgui_render_pass.m_dx_descriptor_heap.Get() };
        m_dx_cmd_list->SetDescriptorHeaps(1, p);
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_dx_cmd_list.Get());
    }

    void
    submit(Fence * fence, Semaphore * semaphore_wait = nullptr, Semaphore * semaphore_signal = nullptr)
    {
        ID3D12CommandList * const command_lists[] = { m_dx_cmd_list.Get() };
        if (semaphore_wait != nullptr)
        {
            DXCK(m_dx_cmd_queue->Wait(semaphore_wait->m_dx_fence.Get(), semaphore_wait->m_expected_fence_value));
        }
        m_dx_cmd_queue->ExecuteCommandLists(_countof(command_lists), command_lists);
        if (fence)
        {
            // make sure the fence is not in the signaled state
            assert(fence->m_dx_fence->GetCompletedValue() != fence->m_expected_fence_value);
            DXCK(m_dx_cmd_queue->Signal(fence->m_dx_fence.Get(), fence->m_expected_fence_value));
            if (semaphore_signal != nullptr)
            {
                DXCK(m_dx_cmd_queue->Signal(semaphore_signal->m_dx_fence.Get(),
                                            semaphore_signal->m_expected_fence_value));
            }
        }
    }

    void
    trace_rays(const RayTracingShaderTable & shader_table,
               const size_t                  width  = 1,
               const size_t                  height = 1,
               const size_t                  depth  = 1)
    {
        // Dispatch rays
        D3D12_DISPATCH_RAYS_DESC desc = shader_table.m_dx_dispatch_rays_desc;
        desc.Width                    = static_cast<UINT>(width);
        desc.Height                   = static_cast<UINT>(height);
        desc.Depth                    = static_cast<UINT>(depth);

        m_dx_cmd_list->DispatchRays(&desc);
    }

    void
    transition_buffer(const Buffer & buffer, const BufferUsageEnum pre_enum, const BufferUsageEnum post_enum)
    {
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Transition.pResource   = buffer.m_allocation->GetResource();
        barrier.Transition.StateBefore = static_cast<D3D12_RESOURCE_STATES>(pre_enum);
        barrier.Transition.StateAfter  = static_cast<D3D12_RESOURCE_STATES>(post_enum);
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        m_dx_cmd_list->ResourceBarrier(1, &barrier);
    }

    void
    transition_texture(const Texture & texture, const TextureStateEnum pre_enum, const TextureStateEnum post_enum)
    {
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Transition.pResource   = texture.m_dx_resource;
        barrier.Transition.StateBefore = static_cast<D3D12_RESOURCE_STATES>(pre_enum);
        barrier.Transition.StateAfter  = static_cast<D3D12_RESOURCE_STATES>(post_enum);
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        m_dx_cmd_list->ResourceBarrier(1, &barrier);
    }

    void
    update_push_constant(const void * data, const size_t size_in_bytes)
    {
        assert(size_in_bytes % 4 == 0);
        m_dx_cmd_list->SetGraphicsRoot32BitConstants(0, static_cast<UINT>(size_in_bytes / 4), data, 0);
    }
};
} // namespace DXA_NAME
#endif
