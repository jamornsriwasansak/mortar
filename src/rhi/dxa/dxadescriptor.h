#pragma once

#include "dxacommon.h"
#ifdef USE_DXA

#include "dxabuffer.h"
#include "dxadescriptorpool.h"
#include "dxarasterpipeline.h"
#include "dxaraytracingaccel.h"
#include "dxaraytracingpipeline.h"
#include "dxasampler.h"
#include "dxatexture.h"
#include "dxilreflection.h"

namespace DXA_NAME
{
struct DescriptorSet
{
    struct RootDescriptorTable
    {
        size_t                      m_root_signature_index;
        D3D12_GPU_DESCRIPTOR_HANDLE m_gpu_handle;
    };

    struct RootSignatureLevelView
    {
        size_t                    m_root_signature_index;
        D3D12_GPU_VIRTUAL_ADDRESS m_virtual_address;
    };

    size_t m_set = std::numeric_limits<size_t>::max();

    // TODO:: purge map and vector. use Ste::FsVector instead
    const std::map<std::tuple<D3D_SHADER_INPUT_TYPE, size_t, size_t>, DxilReflection::DescriptorInfo> * m_descriptor_info =
        nullptr;
    std::map<std::tuple<D3D_SHADER_INPUT_TYPE, size_t, size_t>, DescriptorHandle> m_handles;

    std::vector<RootDescriptorTable>    m_root_descriptor_tables;
    std::vector<RootSignatureLevelView> m_root_cbvs;
    std::vector<RootSignatureLevelView> m_root_srvs;

    DescriptorPool * m_descriptor_pool = nullptr;
    const Device *   m_device          = nullptr;

#ifndef NDEBUG
    bool m_updated = false;
#endif

    DescriptorSet() {}

    DescriptorSet(const Device *                       device,
                  DescriptorPool *                     descriptor_pool,
                  const size_t                         i_set,
                  [[maybe_unused]] const std::string & name = "")
    : m_device(device),
      m_set(i_set),
      m_descriptor_pool(descriptor_pool)
    {
    }

    DescriptorSet(const Device *                       device,
                  const RasterPipeline &               pipeline,
                  DescriptorPool *                     descriptor_pool,
                  const size_t                         i_set,
                  [[maybe_unused]] const std::string & name = "")
    : m_device(device),
      m_set(i_set),
      m_descriptor_pool(descriptor_pool),
      m_descriptor_info(&pipeline.m_descriptor_set_info)
    {
    }

    DescriptorSet(const Device *                       device,
                  const RayTracingPipeline &           pipeline,
                  DescriptorPool *                     descriptor_pool,
                  const size_t                         i_set,
                  [[maybe_unused]] const std::string & name = "")
    : m_device(device),
      m_set(i_set),
      m_descriptor_pool(descriptor_pool),
      m_descriptor_info(&pipeline.m_descriptor_set_info)
    {
    }

    std::optional<DescriptorHandle>
    get_handle(D3D_SHADER_INPUT_TYPE type, DescriptorHeap * heap, const size_t binding, const size_t i_offset)
    {
        const auto index       = std::make_tuple(type, m_set, binding);
        const auto iter        = m_descriptor_info->find(index);
        const auto handle_iter = m_handles.find(index);
        if (iter == m_descriptor_info->end())
        {
            return std::nullopt;
        }

        const auto & desc_info = iter->second;

        assert(i_offset < desc_info.m_num_bindings);

        DescriptorHandle result;
        if (handle_iter == m_handles.end())
        {
            DescriptorHandle handle = heap->get_handle(desc_info.m_num_bindings);
            m_root_descriptor_tables.push_back(
                RootDescriptorTable{ desc_info.m_root_signature_index, handle.m_dx_gpu_handle });
            m_handles[index] = handle;
            result           = handle;
        }
        else
        {
            result = handle_iter->second;
        }

        result.m_dx_cpu_handle.ptr += i_offset * heap->m_dx_handle_size;
        result.m_dx_gpu_handle.ptr += i_offset * heap->m_dx_handle_size;

        return result;
    }

    DescriptorSet &
    set_b_constant_buffer(const size_t binding, const Buffer & buffer)
    {
        assert(HasFlag(buffer.m_buffer_usage, BufferUsageEnum::ConstantBuffer));

        const auto   index     = std::make_tuple(D3D_SIT_CBUFFER, m_set, binding);
        const auto   iter      = m_descriptor_info->find(index);
        const auto & desc_info = iter->second;
        assert(desc_info.m_num_bindings == 1);
        m_root_cbvs.push_back(
            RootSignatureLevelView{ desc_info.m_root_signature_index,
                                    buffer.m_allocation->GetResource()->GetGPUVirtualAddress() });
        return *this;
    }

    DescriptorSet &
    set_t_byte_address_buffer(const size_t   binding,
                              const Buffer & buffer,
                              const size_t   stride,
                              const size_t   num_elements  = 0,
                              const size_t   i_buffer      = 0,
                              const size_t   first_element = 0)
    {
        assert(first_element % 32 == 0);
        assert(buffer.m_size_in_bytes % sizeof(uint32_t) == 0);
        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
        srv_desc.Shader4ComponentMapping         = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv_desc.ViewDimension                   = D3D12_SRV_DIMENSION_BUFFER;
        srv_desc.Format                          = DXGI_FORMAT_R32_TYPELESS;
        srv_desc.Buffer.FirstElement             = first_element;
        srv_desc.Buffer.Flags                    = D3D12_BUFFER_SRV_FLAG_RAW;
        srv_desc.Buffer.StructureByteStride      = 0;
        srv_desc.Buffer.NumElements              = static_cast<UINT>(num_elements);
        std::optional<DescriptorHandle> handle =
            get_handle(D3D_SIT_BYTEADDRESS, &m_descriptor_pool->m_cbv_srv_uav_heap, binding, i_buffer);

        if (handle.has_value())
        {
            m_device->m_dx_device->CreateShaderResourceView(buffer.m_allocation->GetResource(),
                                                            &srv_desc,
                                                            handle->m_dx_cpu_handle);
        }
        else
        {
            Logger::Warn(__FUNCTION__,
                         " cannot set_t_byte_address_buffer(",
                         std::to_string(m_set),
                         ", ",
                         std::to_string(binding),
                         ")");
        }
        return *this;
    }

    DescriptorSet &
    set_t_structured_buffer(const size_t   binding,
                            const Buffer & buffer,
                            const size_t   stride,
                            const size_t   num_elements  = 1,
                            const size_t   i_buffer      = 0,
                            const size_t   first_element = 0)
    {
        assert(first_element % 32 == 0);
        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
        srv_desc.Shader4ComponentMapping         = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv_desc.ViewDimension                   = D3D12_SRV_DIMENSION_BUFFER;
        srv_desc.Format                          = DXGI_FORMAT_UNKNOWN;
        srv_desc.Buffer.FirstElement             = first_element;
        srv_desc.Buffer.Flags                    = D3D12_BUFFER_SRV_FLAG_NONE;
        srv_desc.Buffer.StructureByteStride      = static_cast<UINT>(stride);
        srv_desc.Buffer.NumElements              = static_cast<UINT>(num_elements);
        std::optional<DescriptorHandle> handle =
            get_handle(D3D_SIT_STRUCTURED, &m_descriptor_pool->m_cbv_srv_uav_heap, binding, i_buffer);

        if (handle.has_value())
        {
            m_device->m_dx_device->CreateShaderResourceView(buffer.m_allocation->GetResource(),
                                                            &srv_desc,
                                                            handle->m_dx_cpu_handle);
        }
        else
        {
            Logger::Warn(__FUNCTION__,
                         " cannot set_t_structured_buffer(",
                         std::to_string(m_set),
                         ", ",
                         std::to_string(binding),
                         ")");
        }
        return *this;
    }

    DescriptorSet &
    set_s_sampler(const size_t binding, const Sampler & sampler, const size_t i_sampler = 0)
    {
        std::optional<DescriptorHandle> handle =
            get_handle(D3D_SIT_SAMPLER, &m_descriptor_pool->m_sampler_heap, binding, i_sampler);
        if (handle.has_value())
        {
            m_device->m_dx_device->CreateSampler(&sampler.sampler_desc, handle->m_dx_cpu_handle);
        }
        else
        {
            Logger::Warn(__FUNCTION__,
                         " cannot set_s_sampler(",
                         std::to_string(m_set),
                         ", ",
                         std::to_string(binding),
                         ")");
        }
        return *this;
    }

    DescriptorSet &
    set_t_texture(const size_t binding, const Texture & texture, const size_t i_texture = 0)
    {
        // alloc srv in heap
        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
        srv_desc.Shader4ComponentMapping         = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv_desc.Format                          = texture.get_view_format();
        srv_desc.ViewDimension                   = D3D12_SRV_DIMENSION_TEXTURE2D;
        srv_desc.Texture2D.MipLevels             = 1;
        std::optional<DescriptorHandle> handle =
            get_handle(D3D_SIT_TEXTURE, &m_descriptor_pool->m_cbv_srv_uav_heap, binding, i_texture);
        if (handle.has_value())
        {
            m_device->m_dx_device->CreateShaderResourceView(texture.m_dx_resource, &srv_desc, handle->m_dx_cpu_handle);
        }
        else
        {
            Logger::Warn(__FUNCTION__,
                         " cannot set_t_texture(",
                         std::to_string(m_set),
                         ", ",
                         std::to_string(binding),
                         ")");
        }
        return *this;
    }

    DescriptorSet &
    set_t_ray_tracing_accel(const size_t binding, const RayTracingTlas & tlas)
    {
        /*
        const auto   index     = std::make_tuple(D3D_SIT_RTACCELERATIONSTRUCTURE, m_set, binding);
        const auto   iter      = m_descriptor_info->find(index);
        const auto & desc_info = iter->second;
        assert(desc_info.m_num_bindings == 1);
        m_root_srvs.push_back(RootSignatureLevelView{
            desc_info.m_root_signature_index,
            tlas.m_tlas_buffer.m_allocation->GetResource()->GetGPUVirtualAddress() });
            */
        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
        srv_desc.Shader4ComponentMapping         = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv_desc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
        srv_desc.Format        = DXGI_FORMAT_UNKNOWN;
        srv_desc.RaytracingAccelerationStructure.Location =
            tlas.m_tlas_buffer.m_allocation->GetResource()->GetGPUVirtualAddress();
        std::optional<DescriptorHandle> handle =
            get_handle(D3D_SIT_RTACCELERATIONSTRUCTURE, &m_descriptor_pool->m_cbv_srv_uav_heap, binding, 0);
        if (handle.has_value())
        {
            m_device->m_dx_device->CreateShaderResourceView(nullptr, &srv_desc, handle->m_dx_cpu_handle);
        }
        else
        {
            Logger::Warn(__FUNCTION__,
                         " cannot set_t_ray_tracing_accel(",
                         std::to_string(m_set),
                         ", ",
                         std::to_string(binding),
                         ")");
        }
        return *this;
    }

    DescriptorSet &
    set_u_rw_texture(const size_t binding, const Texture & texture, const size_t i_texture = 0)
    {
        // alloc uav in heap
        D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
        uav_desc.Format                           = texture.get_view_format();
        uav_desc.ViewDimension                    = D3D12_UAV_DIMENSION_TEXTURE2D;
        std::optional<DescriptorHandle> handle =
            get_handle(D3D_SIT_UAV_RWTYPED, &m_descriptor_pool->m_cbv_srv_uav_heap, binding, i_texture);
        if (handle.has_value())
        {
            m_device->m_dx_device->CreateUnorderedAccessView(texture.m_dx_resource,
                                                             nullptr,
                                                             &uav_desc,
                                                             handle->m_dx_cpu_handle);
        }
        else
        {
            Logger::Warn(__FUNCTION__,
                         " cannot set_u_rw_texture(",
                         std::to_string(m_set),
                         ", ",
                         std::to_string(binding),
                         ")");
        }
        return *this;
    }

    DescriptorSet &
    set_u_rw_structured_buffer(const size_t   binding,
                               const Buffer & buffer,
                               const size_t   stride,
                               const size_t   num_elements,
                               const size_t   i_buffer      = 0,
                               const size_t   first_element = 0)
    {
        assert(first_element % 32 == 0);
        D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
        uav_desc.Format                           = DXGI_FORMAT_UNKNOWN;
        uav_desc.Buffer.FirstElement              = first_element;
        uav_desc.Buffer.StructureByteStride       = static_cast<UINT>(stride);
        uav_desc.Buffer.NumElements               = static_cast<UINT>(num_elements);
        uav_desc.ViewDimension                    = D3D12_UAV_DIMENSION_BUFFER;
        std::optional<DescriptorHandle> handle =
            get_handle(D3D_SIT_UAV_RWSTRUCTURED, &m_descriptor_pool->m_cbv_srv_uav_heap, binding, i_buffer);

        if (handle.has_value())
        {
            m_device->m_dx_device->CreateUnorderedAccessView(buffer.m_allocation->GetResource(),
                                                             nullptr,
                                                             &uav_desc,
                                                             handle->m_dx_cpu_handle);
        }
        else
        {
            Logger::Warn(__FUNCTION__,
                         " cannot set_u_rw_structured_buffer(",
                         std::to_string(m_set),
                         ", ",
                         std::to_string(binding),
                         ")");
        }
        return *this;
    }

    void
    update()
    {
#ifndef NDEBUG
        m_updated = true;
#endif
    }
};
} // namespace DXA_NAME
#endif