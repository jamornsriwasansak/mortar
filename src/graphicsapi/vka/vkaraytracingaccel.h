#pragma once

#include "vkabuffer.h"
#include "vkacommon.h"
#include "vkadevice.h"

namespace Vka
{
struct RayTracingGeometryDesc
{
    vk::AccelerationStructureGeometryTrianglesDataKHR m_geometry_trimesh_desc;
    vk::AccelerationStructureBuildRangeInfoKHR        m_build_range;
    vk::GeometryFlagBitsKHR                           m_geometry_flag;

    RayTracingGeometryDesc() {}

    RayTracingGeometryDesc &
    set_vertex_buffer(const Buffer &   buffer,
                      const size_t     num_vertices,
                      const size_t     stride_in_bytes,
                      const FormatEnum vk_format,
                      const size_t     starting_index = 0)
    {
        m_geometry_trimesh_desc.setVertexData(buffer.m_device_address + starting_index * stride_in_bytes);
        m_geometry_trimesh_desc.setVertexFormat(vk::Format(vk_format));
        m_geometry_trimesh_desc.setMaxVertex(static_cast<uint32_t>(num_vertices));
        m_geometry_trimesh_desc.setVertexStride(static_cast<uint32_t>(stride_in_bytes));
        return *this;
    }

    RayTracingGeometryDesc &
    set_index_buffer(const Buffer &  buffer,
                     const size_t    num_indices,
                     const size_t    stride_in_bytes,
                     const IndexType index_type,
                     const size_t    starting_index = 0)
    {
        m_geometry_trimesh_desc.setIndexData(buffer.m_device_address + starting_index * stride_in_bytes);
        m_geometry_trimesh_desc.setIndexType(vk::IndexType(index_type));

        m_build_range.setFirstVertex(0);
        m_build_range.setPrimitiveOffset(0);
        m_build_range.setPrimitiveCount(static_cast<uint32_t>(num_indices / 3));
        m_build_range.setTransformOffset(0);
        return *this;
    }

    RayTracingGeometryDesc &
    set_flag(const RayTracingGeometryFlag flag)
    {
        m_geometry_flag = vk::GeometryFlagBitsKHR(flag);
        return *this;
    }
};

struct RayTracingBlas
{
    Buffer                             m_accel_buffer;
    vk::UniqueAccelerationStructureKHR m_vk_accel_struct;

    RayTracingBlas(const Device *                 device,
                   const RayTracingGeometryDesc * geometry_descs,
                   const size_t                   num_geometries,
                   StagingBufferManager *         buf_manager,
                   const std::string &            name = "")
    {
        std::vector<vk::AccelerationStructureGeometryDataKHR>   tri_geometry_datas(num_geometries);
        std::vector<vk::AccelerationStructureGeometryKHR>       geometries(num_geometries);
        std::vector<vk::AccelerationStructureBuildRangeInfoKHR> build_ranges(num_geometries);
        std::vector<uint32_t> max_primitive_counts(num_geometries);
        for (size_t i = 0; i < num_geometries; i++)
        {
            auto & geometry_data = tri_geometry_datas[i];
            geometry_data.setTriangles(geometry_descs[i].m_geometry_trimesh_desc);

            auto & geometry = geometries[i];
            geometry.setGeometry(geometry_data);
            geometry.setGeometryType(vk::GeometryTypeKHR::eTriangles);
            geometry.setFlags(geometry_descs[i].m_geometry_flag);

            build_ranges[i]         = geometry_descs[i].m_build_range;
            max_primitive_counts[i] = geometry_descs[i].m_build_range.primitiveCount;
        }

        vk::AccelerationStructureBuildGeometryInfoKHR build_info;
        build_info.setMode(vk::BuildAccelerationStructureModeKHR::eBuild);
        build_info.setFlags(vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace);
        build_info.setPGeometries(geometries.data());
        build_info.setGeometryCount(static_cast<uint32_t>(geometries.size()));
        build_info.setType(vk::AccelerationStructureTypeKHR::eBottomLevel);

        // get size requirement
        vk::AccelerationStructureBuildSizesInfoKHR size_info =
            device->m_vk_ldevice->getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice,
                                                                        build_info,
                                                                        max_primitive_counts);

        const vk::DeviceSize required_scratch_size = size_info.buildScratchSize;
        const vk::DeviceSize required_buffer_size  = size_info.accelerationStructureSize;

        // create buffer for storing blas
        vk::BufferCreateInfo buffer_ci_tmp;
        buffer_ci_tmp.setSize(required_buffer_size);
        buffer_ci_tmp.setUsage(vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR);
        VkBufferCreateInfo      buffer_ci    = buffer_ci_tmp;
        VmaAllocationCreateInfo vma_alloc_ci = {};
        vma_alloc_ci.usage                   = VmaMemoryUsage::VMA_MEMORY_USAGE_GPU_ONLY;

        m_accel_buffer = Buffer(device,
                                BufferUsageEnum::RayTracingAccelStructBuffer,
                                MemoryUsageEnum::GpuOnly,
                                required_buffer_size,
                                nullptr,
                                nullptr,
                                name + "_buffer");

        // create acceleration structure
        vk::AccelerationStructureCreateInfoKHR accel_ci = {};
        accel_ci.setBuffer(m_accel_buffer.m_vma_buffer_bundle->m_vk_buffer);
        accel_ci.setType(vk::AccelerationStructureTypeKHR::eBottomLevel);
        accel_ci.setSize(required_buffer_size);
        m_vk_accel_struct = device->m_vk_ldevice->createAccelerationStructureKHRUnique(accel_ci);

        auto * scratch_buffer = buf_manager->get_scratch_buffer(required_scratch_size);
        build_info.setDstAccelerationStructure(m_vk_accel_struct.get());
        build_info.setScratchData(scratch_buffer->m_vk_device_address);

        buf_manager->m_vk_command_buffer->buildAccelerationStructuresKHR({ build_info },
                                                                         { build_ranges.data() });
    }
};

struct RayTracingInstance
{
    vk::AccelerationStructureInstanceKHR m_vk_instance = {};

    RayTracingInstance(RayTracingBlas * blas, const size_t hit_group_index)
    {
        m_vk_instance.transform.matrix[0][0] = 1.0f;
        m_vk_instance.transform.matrix[1][1] = 1.0f;
        m_vk_instance.transform.matrix[2][2] = 1.0f;
        m_vk_instance.setAccelerationStructureReference(blas->m_accel_buffer.m_device_address);
        m_vk_instance.setFlags(vk::GeometryInstanceFlagBitsKHR::eTriangleCullDisable);
        m_vk_instance.setMask(1);
        m_vk_instance.setInstanceShaderBindingTableRecordOffset(static_cast<uint32_t>(hit_group_index));
    }
};

struct RayTracingTlas
{
    Buffer                             m_instance_buffer;
    Buffer                             m_accel_buffer;
    vk::UniqueAccelerationStructureKHR m_vk_accel_struct;

    RayTracingTlas(const Device *             device,
                   const RayTracingInstance * instance,
                   const size_t               num_instances,
                   StagingBufferManager *     buf_manager,
                   const std::string &        name = "")
    {
        std::vector<vk::AccelerationStructureInstanceKHR> instances(num_instances);
        for (size_t i_instance = 0; i_instance < num_instances; i_instance++)
        {
            instances[i_instance] = instance[i_instance].m_vk_instance;
        }

        const std::string instance_buffer_name = name.empty() ? "" : name + "_instance_buffer";
        m_instance_buffer                      = Buffer(device,
                                   BufferUsageEnum::TransferSrc,
                                   MemoryUsageEnum::CpuOnly,
                                   sizeof(vk::AccelerationStructureInstanceKHR) * num_instances,
                                   reinterpret_cast<std::byte *>(instances.data()),
                                   buf_manager,
                                   instance_buffer_name);

        vk::AccelerationStructureGeometryInstancesDataKHR geometry_instance_data;
        geometry_instance_data.setArrayOfPointers(VK_FALSE);
        geometry_instance_data.setData(m_instance_buffer.m_device_address);

        vk::AccelerationStructureGeometryKHR geometry;
        geometry.setGeometryType(vk::GeometryTypeKHR::eInstances);
        geometry.setGeometry(geometry_instance_data);

        // geometry info
        vk::AccelerationStructureBuildGeometryInfoKHR build_info;
        build_info.setMode(vk::BuildAccelerationStructureModeKHR::eBuild);
        build_info.setFlags(vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace);
        build_info.setPGeometries(&geometry);
        build_info.setGeometryCount(1);
        build_info.setType(vk::AccelerationStructureTypeKHR::eTopLevel);

        // get size requirement
        vk::AccelerationStructureBuildSizesInfoKHR size_info =
            device->m_vk_ldevice->getAccelerationStructureBuildSizesKHR(
                vk::AccelerationStructureBuildTypeKHR::eDevice,
                build_info,
                { static_cast<uint32_t>(instances.size()) });

        const vk::DeviceSize required_scratch_size = size_info.buildScratchSize;
        const vk::DeviceSize required_buffer_size  = size_info.accelerationStructureSize;

        // create buffer for storing tlas
        vk::BufferCreateInfo buffer_ci_tmp;
        buffer_ci_tmp.setSize(required_buffer_size);
        buffer_ci_tmp.setUsage(vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR);
        VkBufferCreateInfo      buffer_ci    = buffer_ci_tmp;
        VmaAllocationCreateInfo vma_alloc_ci = {};
        vma_alloc_ci.usage                   = VmaMemoryUsage::VMA_MEMORY_USAGE_GPU_ONLY;

        const std::string accel_buffer_name = name.empty() ? "" : name + "_accel_buffer";
        m_accel_buffer                      = Buffer(device,
                                BufferUsageEnum::RayTracingAccelStructBuffer,
                                MemoryUsageEnum::GpuOnly,
                                required_buffer_size,
                                nullptr,
                                nullptr,
                                accel_buffer_name);

        // create acceleration structure
        vk::AccelerationStructureCreateInfoKHR accel_ci = {};
        accel_ci.setBuffer(m_accel_buffer.m_vma_buffer_bundle->m_vk_buffer);
        accel_ci.setType(vk::AccelerationStructureTypeKHR::eTopLevel);
        accel_ci.setSize(required_buffer_size);
        m_vk_accel_struct = device->m_vk_ldevice->createAccelerationStructureKHRUnique(accel_ci);
        device->name_vkhpp_object<vk::AccelerationStructureKHR, vk::AccelerationStructureKHR::CType>(
            m_vk_accel_struct.get(),
            name);

        auto * scratch_buffer = buf_manager->get_scratch_buffer(required_scratch_size);
        build_info.setDstAccelerationStructure(m_vk_accel_struct.get());
        build_info.setScratchData(scratch_buffer->m_vk_device_address);

        vk::AccelerationStructureBuildRangeInfoKHR build_range = {};
        build_range.setPrimitiveCount(static_cast<uint32_t>(instances.size()));

        // make sure that instance_buffer is completely written before we proceed further.
        vk::MemoryBarrier barrier(vk::AccessFlagBits::eTransferWrite,
                                  vk::AccessFlagBits::eAccelerationStructureWriteKHR);
        buf_manager->m_vk_command_buffer->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                                          vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR,
                                                          vk::DependencyFlags(),
                                                          { barrier },
                                                          {},
                                                          {});

        buf_manager->m_vk_command_buffer->buildAccelerationStructuresKHR({ build_info }, { &build_range });
    }
};
} // namespace Vka