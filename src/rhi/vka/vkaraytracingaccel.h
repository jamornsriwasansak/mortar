#pragma once

#include "vkacommon.h"

#ifdef USE_VKA

#include "vkabuffer.h"
#include "vkadevice.h"

namespace VKA_NAME
{
struct RayTracingGeometryDesc
{
    vk::AccelerationStructureGeometryTrianglesDataKHR m_geometry_trimesh_desc;
    vk::AccelerationStructureBuildRangeInfoKHR        m_build_range;
    vk::GeometryFlagBitsKHR                           m_geometry_flag;

    RayTracingGeometryDesc() {}

    RayTracingGeometryDesc &
    set_vertex_buffer(const Buffer &   buffer,
                      const size_t     offset_in_bytes,
                      const FormatEnum vk_format,
                      const size_t     stride_in_bytes,
                      const size_t     num_vertices)
    {
        assert(num_vertices > 0);
        m_geometry_trimesh_desc.setVertexData(buffer.m_device_address + offset_in_bytes);
        m_geometry_trimesh_desc.setVertexFormat(vk::Format(vk_format));
        m_geometry_trimesh_desc.setMaxVertex(static_cast<uint32_t>(num_vertices));
        m_geometry_trimesh_desc.setVertexStride(static_cast<uint32_t>(stride_in_bytes));
        return *this;
    }

    RayTracingGeometryDesc &
    set_index_buffer(const Buffer & buffer, const size_t offset_in_bytes, const IndexType index_type, const size_t num_indices)
    {
        m_geometry_trimesh_desc.setIndexData(buffer.m_device_address + offset_in_bytes);
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

    RayTracingGeometryDesc &
    set_transform_matrix4x3(const Buffer & buffer, const size_t offset_in_bytes)
    {
        m_geometry_trimesh_desc.setTransformData(buffer.m_device_address + offset_in_bytes);
    }
};

struct RayTracingBlas
{
    Buffer                             m_accel_buffer;
    vk::UniqueAccelerationStructureKHR m_vk_accel_struct;

    RayTracingBlas() {}

    RayTracingBlas(const std::string &                             name,
                   const Device &                                  device,
                   const std::span<const RayTracingGeometryDesc> & geometry_descs,
                   const RayTracingBuildHint                       hint,
                   StagingBufferManager * buf_manager // TODO:: get rid of staging buffer manager
    )
    {
        std::vector<vk::AccelerationStructureGeometryDataKHR> tri_geometry_datas(geometry_descs.size());
        std::vector<vk::AccelerationStructureGeometryKHR>     geometries(geometry_descs.size());
        std::vector<vk::AccelerationStructureBuildRangeInfoKHR> build_ranges(geometry_descs.size());
        std::vector<uint32_t> max_primitive_counts(geometry_descs.size());
        for (size_t i = 0; i < geometry_descs.size(); i++)
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

        vk::BuildAccelerationStructureFlagsKHR build_flags(
            static_cast<VkBuildAccelerationStructureFlagBitsKHR>(hint));

        vk::AccelerationStructureBuildGeometryInfoKHR build_info;
        build_info.setMode(vk::BuildAccelerationStructureModeKHR::eBuild);
        build_info.setFlags(build_flags);
        build_info.setPGeometries(geometries.data());
        build_info.setGeometryCount(static_cast<uint32_t>(geometries.size()));
        build_info.setType(vk::AccelerationStructureTypeKHR::eBottomLevel);

        // get size requirement
        vk::AccelerationStructureBuildSizesInfoKHR size_info =
            device.m_vk_ldevice->getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice,
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

        m_accel_buffer = Buffer(name + "_buffer",
                                device,
                                BufferUsageEnum::RayTracingAccelStructBuffer,
                                MemoryUsageEnum::GpuOnly,
                                required_buffer_size);

        // create acceleration structure
        vk::AccelerationStructureCreateInfoKHR accel_ci = {};
        accel_ci.setBuffer(static_cast<vk::Buffer>(m_accel_buffer.m_vma_buffer_bundle->m_vk_buffer));
        accel_ci.setType(vk::AccelerationStructureTypeKHR::eBottomLevel);
        accel_ci.setSize(required_buffer_size);
        m_vk_accel_struct = device.m_vk_ldevice->createAccelerationStructureKHRUnique(accel_ci);

        auto * scratch_buffer = buf_manager->get_scratch_buffer(required_scratch_size);
        build_info.setDstAccelerationStructure(m_vk_accel_struct.get());
        build_info.setScratchData(scratch_buffer->m_vk_device_address);

        buf_manager->m_vk_command_buffer.buildAccelerationStructuresKHR({ build_info },
                                                                        { build_ranges.data() });

        // TODO:: do compaction if Flag is ePreferFastTrace
    }
};

struct RayTracingInstance
{
    vk::AccelerationStructureInstanceKHR m_vk_instance = {};

    RayTracingInstance() {}

    RayTracingInstance(const RayTracingBlas & blas, const float4x4 transform, const uint32_t hit_group_index, const uint32_t instance_id)
    {
        // create an instance desc for the bottom-level acceleration structure.
        for (uint8_t r = 0; r < 3; r++)
            for (uint8_t c = 0; c < 4; c++)
            {
                m_vk_instance.transform.matrix[r][c] = transform[c][r];
            }
        m_vk_instance.setAccelerationStructureReference(blas.m_accel_buffer.m_device_address);
        m_vk_instance.setFlags(vk::GeometryInstanceFlagBitsKHR::eTriangleCullDisable);
        m_vk_instance.setMask(1);
        m_vk_instance.setInstanceShaderBindingTableRecordOffset(hit_group_index);
        m_vk_instance.setInstanceCustomIndex(instance_id);
    }
};

struct RayTracingTlas
{
    Buffer                             m_instance_buffer;
    Buffer                             m_accel_buffer;
    vk::UniqueAccelerationStructureKHR m_vk_accel_struct;

    RayTracingTlas() {}

    RayTracingTlas(const std::string &                         name,
                   const Device &                              device,
                   const std::span<const RayTracingInstance> & instances,
                   StagingBufferManager *                      buf_manager)
    {
        m_instance_buffer = Buffer(name + "_instance_buffer",
                                   device,
                                   BufferUsageEnum::TransferSrc,
                                   MemoryUsageEnum::CpuOnly,
                                   sizeof(vk::AccelerationStructureInstanceKHR) * instances.size());

        std::byte * instance_dst = reinterpret_cast<std::byte *>(m_instance_buffer.map());
        for (size_t i = 0; i < instances.size(); i++)
        {
            size_t offset = i * sizeof(vk::AccelerationStructureInstanceKHR);
            std::memcpy(instance_dst + offset, &instances[i].m_vk_instance, sizeof(vk::AccelerationStructureInstanceKHR));
        }
        m_instance_buffer.unmap();

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
            device.m_vk_ldevice->getAccelerationStructureBuildSizesKHR(
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

        m_accel_buffer = Buffer(name + "_accel_buffer",
                                device,
                                BufferUsageEnum::RayTracingAccelStructBuffer,
                                MemoryUsageEnum::GpuOnly,
                                required_buffer_size);

        // create acceleration structure
        vk::AccelerationStructureCreateInfoKHR accel_ci = {};
        accel_ci.setBuffer(static_cast<vk::Buffer>(m_accel_buffer.m_vma_buffer_bundle->m_vk_buffer));
        accel_ci.setType(vk::AccelerationStructureTypeKHR::eTopLevel);
        accel_ci.setSize(required_buffer_size);
        m_vk_accel_struct = device.m_vk_ldevice->createAccelerationStructureKHRUnique(accel_ci);
        device.name_vkhpp_object<vk::AccelerationStructureKHR, vk::AccelerationStructureKHR::CType>(
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
        buf_manager->m_vk_command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                                         vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR,
                                                         vk::DependencyFlags(),
                                                         { barrier },
                                                         {},
                                                         {});

        buf_manager->m_vk_command_buffer.buildAccelerationStructuresKHR({ build_info }, { &build_range });
    }
};
} // namespace VKA_NAME
#endif