#pragma once

#include <vulkan/vulkan.hpp>

#include "gavulkan/buffer.h"

// Ray Tracing - bottom level accel struct (for triangles of a single mesh)
// one BLAS per triangle mesh for simplicity
struct RtBlas
{
	vk::UniqueAccelerationStructureKHR	m_vk_accel_struct;
	vk::UniqueDeviceMemory				m_vk_memory;

	RtBlas()
	{
	}

	RtBlas(const Buffer & vertex_buffer,
		   const Buffer & index_buffer,
		   const bool is_opaque)
	{
		auto build_blas_result = build_blas(vertex_buffer,
											index_buffer,
											is_opaque);
		m_vk_accel_struct = std::move(build_blas_result.m_vk_accel_struct);
		m_vk_memory = std::move(build_blas_result.m_vk_memory);
	}

	struct ReturnType_build_blas
	{
		vk::UniqueAccelerationStructureKHR	m_vk_accel_struct;
		vk::UniqueDeviceMemory				m_vk_memory;
	};
	ReturnType_build_blas
	build_blas(const Buffer & vertex_buffer,
			   const Buffer & index_buffer, 
			   const bool is_opaque)
	{
		// setup info related to triangle mesh
		vk::AccelerationStructureCreateGeometryTypeInfoKHR accel_geometry_ci;
		{
			accel_geometry_ci.setGeometryType(vk::GeometryTypeKHR::eTriangles);
			accel_geometry_ci.setIndexType(index_buffer.get_index_type());
			accel_geometry_ci.setVertexFormat(vk::Format::eR32G32B32Sfloat);
			accel_geometry_ci.setMaxPrimitiveCount(static_cast<uint32_t>(index_buffer.get_num_indices() / 3));
			accel_geometry_ci.setAllowsTransforms(VK_FALSE);
		}
		vk::AccelerationStructureGeometryTrianglesDataKHR triangles;
		{
			const vk::DeviceAddress vertex_buffer_address = Core::Inst().m_vk_device->getBufferAddress(*vertex_buffer.m_vk_buffer);
			const vk::DeviceAddress index_buffer_address = Core::Inst().m_vk_device->getBufferAddress(*index_buffer.m_vk_buffer);
			triangles.setVertexData(vertex_buffer_address);
			triangles.setVertexStride(vertex_buffer.m_element_size_in_bytes);
			triangles.setVertexFormat(accel_geometry_ci.vertexFormat);
			triangles.setIndexData(index_buffer_address);
			triangles.setIndexType(index_buffer.get_index_type());
			triangles.setTransformData({});
		}
		vk::AccelerationStructureGeometryKHR geometry_data;
		{
			auto geometry_flag = is_opaque ? vk::GeometryFlagBitsKHR::eOpaque : vk::GeometryFlagBitsKHR::eNoDuplicateAnyHitInvocation;
			geometry_data.setGeometryType(vk::GeometryTypeKHR::eTriangles);
			geometry_data.setFlags(geometry_flag);
			geometry_data.geometry.setTriangles(triangles);
		}

		const vk::AccelerationStructureGeometryKHR * p_geometry_datas = &geometry_data;

		// create info for accel structure
		vk::AccelerationStructureCreateInfoKHR as_ci = {};
		{
			as_ci.setType(vk::AccelerationStructureTypeKHR::eBottomLevel);
			as_ci.setFlags(vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace);
			as_ci.setMaxGeometryCount(1);
			as_ci.setPGeometryInfos(&accel_geometry_ci);
		}

		// create accel
		vk::UniqueAccelerationStructureKHR accel = Core::Inst().m_vk_device->createAccelerationStructureKHRUnique(as_ci);
		vk::UniqueDeviceMemory memory = alloc_and_bind_memory(*accel);

		// find memory requirement for scratch buffer
		vk::AccelerationStructureMemoryRequirementsInfoKHR mem_req_info;
		{
			mem_req_info.setType(vk::AccelerationStructureMemoryRequirementsTypeKHR::eBuildScratch);
			mem_req_info.setBuildType(vk::AccelerationStructureBuildTypeKHR::eDevice);
			mem_req_info.setAccelerationStructure(*accel);
		}
		vk::MemoryRequirements mem_req = (*Core::Inst().m_vk_device).getAccelerationStructureMemoryRequirementsKHR(mem_req_info)
																	.memoryRequirements;

		// create scratch buffer
		Buffer scratch_buffer(static_cast<uint32_t>(mem_req.size),
							  vk::MemoryPropertyFlagBits::eDeviceLocal,
							  vk::BufferUsageFlagBits::eRayTracingKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress);
		const vk::DeviceAddress scratch_buffer_address = Core::Inst().m_vk_device->getBufferAddress(*scratch_buffer.m_vk_buffer);

		vk::AccelerationStructureBuildGeometryInfoKHR accel_build_geometry_info;
		accel_build_geometry_info.setType(vk::AccelerationStructureTypeKHR::eBottomLevel);
		accel_build_geometry_info.setFlags(vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace);
		accel_build_geometry_info.setUpdate(VK_FALSE);
		accel_build_geometry_info.setSrcAccelerationStructure(nullptr);
		accel_build_geometry_info.setDstAccelerationStructure(*accel);
		accel_build_geometry_info.setGeometryArrayOfPointers(VK_FALSE);
		accel_build_geometry_info.setGeometryCount(1);
		accel_build_geometry_info.setPpGeometries(&p_geometry_datas);
		accel_build_geometry_info.setScratchData(scratch_buffer_address);

		vk::AccelerationStructureBuildOffsetInfoKHR offset;
		{
			offset.setFirstVertex(0);
			offset.setPrimitiveCount(accel_geometry_ci.maxPrimitiveCount);
			offset.setPrimitiveOffset(0);
		}
		const vk::AccelerationStructureBuildOffsetInfoKHR * p_offset = &offset;

		// build accel
		Core::Inst().one_time_command_submit(
			[&](vk::CommandBuffer & command_buffer)
			{
				command_buffer.buildAccelerationStructureKHR(1u, &accel_build_geometry_info, &p_offset);
				vk::MemoryBarrier barrier;
				{
					barrier.setSrcAccessMask(vk::AccessFlagBits::eAccelerationStructureReadKHR);
					barrier.setDstAccessMask(vk::AccessFlagBits::eAccelerationStructureWriteKHR);
				}
				command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR,
											   vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR,
											   vk::DependencyFlags(),
											   { barrier },
											   nullptr,
											   nullptr);
			});

		return { std::move(accel), std::move(memory) };
	}

	vk::UniqueDeviceMemory
	alloc_and_bind_memory(const vk::AccelerationStructureKHR & vk_accel_struct)
	{
		// find memory requirement
		vk::AccelerationStructureMemoryRequirementsInfoKHR mem_req_info;
		{
			mem_req_info.setAccelerationStructure(vk_accel_struct);
			mem_req_info.setBuildType(vk::AccelerationStructureBuildTypeKHR::eDevice);
			mem_req_info.setType(vk::AccelerationStructureMemoryRequirementsTypeKHR::eObject);
		}
		vk::MemoryRequirements mem_req = (*Core::Inst().m_vk_device).getAccelerationStructureMemoryRequirementsKHR(mem_req_info)
																	.memoryRequirements;

		// allocate memory
		vk::MemoryAllocateInfo mem_alloc_info;
		{
			mem_alloc_info.setAllocationSize(mem_req.size);
			mem_alloc_info.setMemoryTypeIndex(find_mem_type(Core::Inst().m_vk_physical_device,
															mem_req.memoryTypeBits,
															vk::MemoryPropertyFlagBits::eDeviceLocal));
		}
		vk::UniqueDeviceMemory memory = Core::Inst().m_vk_device->allocateMemoryUnique(mem_alloc_info);

		// bind memory with accel structure
		vk::BindAccelerationStructureMemoryInfoKHR bind_accel_mem;
		{
			bind_accel_mem.setAccelerationStructure(vk_accel_struct);
			bind_accel_mem.setMemory(*memory);
			bind_accel_mem.setMemoryOffset(0);
		}
		Core::Inst().m_vk_device->bindAccelerationStructureMemoryKHR(bind_accel_mem);
		return memory;
	}
};


// Ray Tracing - top level accel struct (for triangles of a single mesh)
struct RtTlas
{
	vk::UniqueAccelerationStructureKHR	m_vk_accel_struct;
	vk::UniqueDeviceMemory				m_vk_memory;

	RtTlas()
	{
	}

	RtTlas(const std::vector<RtBlas *> & rt_blases)
	{
		std::vector<vk::AccelerationStructureKHR> blases(rt_blases.size());
		for (size_t i_blas = 0; i_blas < blases.size(); i_blas++)
		{
			blases[i_blas] = *rt_blases[i_blas]->m_vk_accel_struct;
		}
		auto tlas_result = create_tlas(blases);
		m_vk_accel_struct = std::move(tlas_result.m_vk_accel_struct);
		m_vk_memory = std::move(tlas_result.m_vk_memory);
	}

	struct ReturnType_create_tlas
	{
		vk::UniqueAccelerationStructureKHR	m_vk_accel_struct;
		vk::UniqueDeviceMemory				m_vk_memory;
	};
	ReturnType_create_tlas
	create_tlas(const std::vector<vk::AccelerationStructureKHR> & instances)
	{
		vk::AccelerationStructureCreateGeometryTypeInfoKHR geometry_type_info;
		{
			geometry_type_info.setAllowsTransforms(VK_FALSE);
			geometry_type_info.setGeometryType(vk::GeometryTypeKHR::eInstances);
			geometry_type_info.setMaxPrimitiveCount(static_cast<uint32_t>(instances.size()));
		}

		// create empty tlas
		vk::Device & vk_device = *Core::Inst().m_vk_device;
		vk::AccelerationStructureCreateInfoKHR as_ci;
		{
			as_ci.setCompactedSize(0);
			as_ci.setPGeometryInfos(&geometry_type_info);
			as_ci.setType(vk::AccelerationStructureTypeKHR::eTopLevel);
			as_ci.setMaxGeometryCount(1);
			as_ci.setFlags(vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace);
		}
		vk::UniqueAccelerationStructureKHR tlas = vk_device.createAccelerationStructureKHRUnique(as_ci);
		vk::UniqueDeviceMemory memory = alloc_and_bind_memory(*tlas);

		// find out scratch buffer size
		vk::AccelerationStructureMemoryRequirementsInfoKHR mem_req_info;
		{
			mem_req_info.setType(vk::AccelerationStructureMemoryRequirementsTypeKHR::eBuildScratch);
			mem_req_info.setBuildType(vk::AccelerationStructureBuildTypeKHR::eDevice);
			mem_req_info.setAccelerationStructure(*tlas);
		}
		vk::DeviceSize scratch_size = vk_device.getAccelerationStructureMemoryRequirementsKHR(mem_req_info)
											   .memoryRequirements.size;

		// create scratch buffer
		Buffer scratch_buffer(scratch_size,
							  vk::MemoryPropertyFlagBits::eDeviceLocal,
							  vk::BufferUsageFlagBits::eRayTracingKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress);

		// build instance descriptor (pointer to geometry) for each instance
		std::vector<vk::AccelerationStructureInstanceKHR> geometry_instances;
		geometry_instances.reserve(instances.size());
		for (size_t i_instance = 0; i_instance < instances.size(); i_instance++)
		{
			vk::DeviceAddress instance_address = vk_device.getAccelerationStructureAddressKHR(instances[i_instance]);

			// TODO:: transform!
			vk::TransformMatrixKHR transform = std::array<std::array<float, 4>, 3> {
				1.0f, 0.0f, 0.0f, 0.0f,
				0.0f, 1.0f, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f, 0.0f
			};

			vk::AccelerationStructureInstanceKHR accel_instance;
			{
				accel_instance.setAccelerationStructureReference(instance_address);
				accel_instance.setFlags(vk::GeometryInstanceFlagBitsKHR::eTriangleCullDisable);
				accel_instance.setInstanceCustomIndex(static_cast<uint32_t>(i_instance));
				accel_instance.setInstanceShaderBindingTableRecordOffset(0u);
				accel_instance.setMask(0xFF);
				accel_instance.setTransform(transform);
			}
			geometry_instances.push_back(accel_instance);
		}

		// transfer the instance descriptors to GPU via buffer
		Buffer instance_buffer(geometry_instances,
							   vk::MemoryPropertyFlagBits::eDeviceLocal,
							   vk::BufferUsageFlagBits::eRayTracingKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress);
		vk::DeviceAddress instance_buffer_address = vk_device.getBufferAddress(*instance_buffer.m_vk_buffer);

		vk::AccelerationStructureGeometryKHR geometry;
		geometry.setFlags(vk::GeometryFlagBitsKHR::eOpaque);
		geometry.geometry.instances.arrayOfPointers = VK_FALSE;
		geometry.geometry.instances.data.deviceAddress = instance_buffer_address;
		geometry.setGeometryType(vk::GeometryTypeKHR::eInstances);

		vk::AccelerationStructureGeometryKHR * pGeometry = &geometry;

		vk::AccelerationStructureBuildOffsetInfoKHR offsetInfo;
		offsetInfo.primitiveCount = static_cast<uint32_t>(instances.size());
		offsetInfo.primitiveOffset = 0;
		offsetInfo.firstVertex = 0;
		offsetInfo.transformOffset = 0;

		vk::AccelerationStructureBuildOffsetInfoKHR * pOffsetInfo = &offsetInfo;

		vk::AccelerationStructureBuildGeometryInfoKHR geometryInfo;
		geometryInfo.type = vk::AccelerationStructureTypeKHR::eTopLevel;
		geometryInfo.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
		geometryInfo.update = VK_FALSE;
		geometryInfo.srcAccelerationStructure = nullptr;
		geometryInfo.dstAccelerationStructure = *tlas;
		geometryInfo.geometryArrayOfPointers = VK_FALSE;
		geometryInfo.geometryCount = 1;
		geometryInfo.ppGeometries = &pGeometry;
		geometryInfo.scratchData.deviceAddress = vk_device.getBufferAddress(*scratch_buffer.m_vk_buffer);

		// build tlas
		Core::Inst().one_time_command_submit(
			[&](vk::CommandBuffer & command_buffer)
			{
				// make sure that instance_buffer is completely written before we proceed further.
				vk::MemoryBarrier barrier(vk::AccessFlagBits::eTransferWrite,
										  vk::AccessFlagBits::eAccelerationStructureWriteKHR);
				command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
											   vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR,
											   vk::DependencyFlags(),
											   { barrier },
											   {},
											   {});
				
				// build tlas
				command_buffer.buildAccelerationStructureKHR({ geometryInfo }, { pOffsetInfo });
			});

		return { std::move(tlas), std::move(memory) };
	}

	vk::UniqueDeviceMemory
	alloc_and_bind_memory(const vk::AccelerationStructureKHR & vk_accel_struct)
	{
		// find memory requirement
		vk::AccelerationStructureMemoryRequirementsInfoKHR mem_req_info;
		{
			mem_req_info.setAccelerationStructure(vk_accel_struct);
		}
		vk::MemoryRequirements mem_req = (*Core::Inst().m_vk_device).getAccelerationStructureMemoryRequirementsKHR(mem_req_info)
																	.memoryRequirements;

		// allocate memory
		vk::MemoryAllocateInfo mem_alloc_info;
		{
			mem_alloc_info.setAllocationSize(mem_req.size);
			mem_alloc_info.setMemoryTypeIndex(find_mem_type(Core::Inst().m_vk_physical_device,
															mem_req.memoryTypeBits,
															vk::MemoryPropertyFlagBits::eDeviceLocal));
		}
		vk::UniqueDeviceMemory memory = Core::Inst().m_vk_device->allocateMemoryUnique(mem_alloc_info);

		// bind memory with accel structure
		vk::BindAccelerationStructureMemoryInfoKHR bind_accel_mem;
		{
			bind_accel_mem.setAccelerationStructure(vk_accel_struct);
			bind_accel_mem.setMemory(*memory);
			bind_accel_mem.setMemoryOffset(0);
		}
		Core::Inst().m_vk_device->bindAccelerationStructureMemoryKHR(bind_accel_mem);
		return memory;
	}
};