#pragma once

#include "dxabuffer.h"
#include "dxacommon.h"
#include "dxastagingbuffermanager.h"

namespace DXA_NAME
{
struct RayTracingGeometryDesc
{
    D3D12_RAYTRACING_GEOMETRY_DESC m_geometry_desc;

    RayTracingGeometryDesc() : m_geometry_desc({}) {}

    RayTracingGeometryDesc &
    set_vertex_buffer(const Buffer &   buffer,
                      const size_t     offset_in_bytes,
                      const FormatEnum dxgi_format,
                      const size_t     stride_in_bytes,
                      const size_t     num_vertices)
    {
        m_geometry_desc.Triangles.VertexBuffer.StartAddress =
            buffer.m_allocation->GetResource()->GetGPUVirtualAddress() + offset_in_bytes;
        m_geometry_desc.Triangles.VertexBuffer.StrideInBytes = static_cast<UINT>(stride_in_bytes);
        m_geometry_desc.Triangles.VertexCount                = static_cast<UINT>(num_vertices);
        m_geometry_desc.Triangles.VertexFormat = static_cast<DXGI_FORMAT>(dxgi_format);
        return *this;
    }

    RayTracingGeometryDesc &
    set_index_buffer(const Buffer & buffer, const size_t offset_in_bytes, const IndexType dxgi_format, const size_t num_indices)
    {
        m_geometry_desc.Triangles.IndexBuffer =
            buffer.m_allocation->GetResource()->GetGPUVirtualAddress() + offset_in_bytes;
        m_geometry_desc.Triangles.IndexCount  = static_cast<UINT>(num_indices);
        m_geometry_desc.Triangles.IndexFormat = static_cast<DXGI_FORMAT>(dxgi_format);
        return *this;
    }

    RayTracingGeometryDesc &
    set_flag(const RayTracingGeometryFlag flag)
    {
        m_geometry_desc.Flags = static_cast<D3D12_RAYTRACING_GEOMETRY_FLAGS>(flag);
        return *this;
    }

    RayTracingGeometryDesc &
    set_transform_matrix4x3(const Buffer & buffer, const size_t offset_in_bytes)
    {
        m_geometry_desc.Triangles.Transform3x4 =
            buffer.m_allocation->GetResource()->GetGPUVirtualAddress() + offset_in_bytes;
    }
};

static_assert(sizeof(D3D12_RAYTRACING_GEOMETRY_DESC) == sizeof(RayTracingGeometryDesc));

struct RayTracingBlas
{
    Buffer m_blas_buffer;

    RayTracingBlas() {}

    RayTracingBlas(const Device *                 device,
                   const RayTracingGeometryDesc * geometry_descs,
                   const size_t                   num_geometries,
                   StagingBufferManager *         resource_loader,
                   const std::string &            name = "")
    {
        // setup input building blas
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS bottom_level_inputs = {};
        bottom_level_inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        bottom_level_inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
        bottom_level_inputs.pGeometryDescs =
            reinterpret_cast<const D3D12_RAYTRACING_GEOMETRY_DESC *>(geometry_descs);
        bottom_level_inputs.NumDescs = static_cast<UINT>(num_geometries);
        bottom_level_inputs.Type     = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;

        // get prebuild information
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO bottom_level_prebuild_info = {};
        device->m_dx_device->GetRaytracingAccelerationStructurePrebuildInfo(&bottom_level_inputs,
                                                                            &bottom_level_prebuild_info);
        if (bottom_level_prebuild_info.ResultDataMaxSizeInBytes <= 0)
        {
            Logger::Critical<true>(__FUNCTION__ " bottomlevel's ResultDataMaxSizeInBytes <= 0");
        }

        // setup blas buffer
        m_blas_buffer = Buffer(device,
                               BufferUsageEnum::RayTracingAccelStructBuffer,
                               MemoryUsageEnum::GpuOnly,
                               bottom_level_prebuild_info.ResultDataMaxSizeInBytes,
                               nullptr,
                               nullptr,
                               name);

        auto * scratch_buffer =
            resource_loader->get_scratch_buffer(bottom_level_prebuild_info.ResultDataMaxSizeInBytes);

        // Bottom Level Acceleration Structure desc
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC blas_build_desc = {};
        {
            blas_build_desc.Inputs = bottom_level_inputs;
            blas_build_desc.ScratchAccelerationStructureData =
                scratch_buffer->m_allocation->GetResource()->GetGPUVirtualAddress();
            blas_build_desc.DestAccelerationStructureData =
                m_blas_buffer.m_allocation->GetResource()->GetGPUVirtualAddress();
        }

        // create command list and record commands
        resource_loader->m_dx_command_list->BuildRaytracingAccelerationStructure(&blas_build_desc, 0, nullptr);
        CD3DX12_RESOURCE_BARRIER uav_barrier =
            CD3DX12_RESOURCE_BARRIER::UAV(m_blas_buffer.m_allocation->GetResource());
        resource_loader->m_dx_command_list->ResourceBarrier(1, &uav_barrier);
    }
};

struct RayTracingInstance
{
    D3D12_RAYTRACING_INSTANCE_DESC m_instance_desc = {};

    RayTracingInstance() {}

    RayTracingInstance(RayTracingBlas * blas, size_t hit_group_index)
    {
        // create an instance desc for the bottom-level acceleration structure.
        m_instance_desc.Transform[0][0]     = m_instance_desc.Transform[1][1] =
            m_instance_desc.Transform[2][2] = 1;
        m_instance_desc.InstanceMask        = 1;
        m_instance_desc.AccelerationStructure =
            blas->m_blas_buffer.m_allocation->GetResource()->GetGPUVirtualAddress();
        m_instance_desc.InstanceContributionToHitGroupIndex = hit_group_index;
    }
};

struct RayTracingTlas
{
    Buffer m_tlas_buffer;
    Buffer m_instance_desc_buffer;

    RayTracingTlas() {}

    RayTracingTlas(const Device *                              device,
                   const std::span<const RayTracingInstance *> instances,
                   StagingBufferManager *                      temp_resource_manager,
                   const std::string &                         name = "")
    {
        // copy description of instance into the buffer
        const std::string instance_buffer_name = name.empty() ? "" : name + "_instance_buffer";
        m_instance_desc_buffer                 = Buffer(device,
                                        BufferUsageEnum::None,
                                        MemoryUsageEnum::CpuOnly,
                                        sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * instances.size(),
                                        nullptr,
                                        nullptr,
                                        instance_buffer_name);
        std::byte * instance_dst = reinterpret_cast<std::byte *>(m_instance_desc_buffer.map());
        for (size_t i = 0; i < instances.size(); i++)
        {
            size_t offset = i * sizeof(D3D12_RAYTRACING_INSTANCE_DESC);
            std::memcpy(instance_dst + offset, &instances[i]->m_instance_desc, sizeof(D3D12_RAYTRACING_INSTANCE_DESC));
        }
        m_instance_desc_buffer.unmap();

        // create top level inputs
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS top_level_inputs = {};
        top_level_inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        top_level_inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
        top_level_inputs.InstanceDescs =
            m_instance_desc_buffer.m_allocation->GetResource()->GetGPUVirtualAddress();
        top_level_inputs.NumDescs = static_cast<UINT>(instances.size());
        top_level_inputs.Type     = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

        // tlas input info
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO top_level_prebuild_info = {};
        device->m_dx_device->GetRaytracingAccelerationStructurePrebuildInfo(&top_level_inputs,
                                                                            &top_level_prebuild_info);
        if (top_level_prebuild_info.ResultDataMaxSizeInBytes <= 0)
        {
            Logger::Critical<true>(__FUNCTION__ " toplevel's ResultDataMaxSizeInBytes <= 0");
        }

        auto * scratch_buffer =
            temp_resource_manager->get_scratch_buffer(top_level_prebuild_info.ResultDataMaxSizeInBytes);

        const std::string accel_buffer_name = name.empty() ? "" : name + "_accel_buffer";
        m_tlas_buffer                       = Buffer(device,
                               BufferUsageEnum::RayTracingAccelStructBuffer,
                               MemoryUsageEnum::GpuOnly,
                               top_level_prebuild_info.ResultDataMaxSizeInBytes,
                               nullptr,
                               nullptr,
                               accel_buffer_name);

        // tlas desc
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC tlas_build_desc = {};
        tlas_build_desc.Inputs                                             = top_level_inputs;
        tlas_build_desc.DestAccelerationStructureData =
            m_tlas_buffer.m_allocation->GetResource()->GetGPUVirtualAddress();
        tlas_build_desc.ScratchAccelerationStructureData =
            scratch_buffer->m_allocation->GetResource()->GetGPUVirtualAddress();

        // create commandlist and record the commands
        temp_resource_manager->m_dx_command_list->BuildRaytracingAccelerationStructure(&tlas_build_desc, 0, nullptr);
    }
};
} // namespace DXA_NAME