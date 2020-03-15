#pragma once

#include <vulkan/vulkan.hpp>

#include "gavulkan/buffer.h"

// Ray Tracing - bottom level accel struct (for triangles of a single mesh)
// one BLAS per triangle mesh for simplicity
struct RtBlas
{
	vk::UniqueAccelerationStructureNV	m_vk_accel_struct;
	vk::UniqueDeviceMemory				m_vk_memory;

	RtBlas()
	{
	}

	RtBlas(const Buffer & vertex_buffer,
		   const Buffer & index_buffer,
		   const bool is_opaque)
	{
		vk::GeometryNV geometry_nv = get_geometry_nv(vertex_buffer,
													 index_buffer,
													 is_opaque);
		auto build_blas_result = build_blas(geometry_nv);
		m_vk_accel_struct = std::move(build_blas_result.m_vk_accel_struct);
		m_vk_memory = std::move(build_blas_result.m_vk_memory);
	}

	vk::GeometryNV
	get_geometry_nv(const Buffer & vertex_buffer,
					const Buffer & index_buffer,
					const bool is_opaque)
	{
		vk::GeometryTrianglesNV triangles;
		triangles.setVertexData(*vertex_buffer.m_vk_buffer);
		triangles.setVertexOffset(0);
		triangles.setVertexCount(vertex_buffer.m_num_elements);
		triangles.setVertexStride(vertex_buffer.m_element_size_in_bytes);
		triangles.setVertexFormat(vertex_buffer.m_vk_format);
		triangles.setIndexData(*index_buffer.m_vk_buffer);
		triangles.setIndexOffset(0);
		triangles.setIndexCount(index_buffer.m_num_elements);
		triangles.setIndexType(index_buffer.get_index_type());
		
		vk::GeometryDataNV geometry_data;
		geometry_data.setTriangles(triangles);

		vk::GeometryNV geometry;
		geometry.setGeometry(geometry_data);
		if (is_opaque)
		{
			geometry.setFlags(vk::GeometryFlagBitsNV::eOpaque);
		}
		else
		{ 
			geometry.setFlags(vk::GeometryFlagBitsNV::eNoDuplicateAnyHitInvocation);
		}

		return geometry;
	}

	struct ReturnType_build_blas
	{
		vk::UniqueAccelerationStructureNV	m_vk_accel_struct;
		vk::UniqueDeviceMemory				m_vk_memory;
	};
	ReturnType_build_blas
	build_blas(const vk::GeometryNV & vk_geometry_nvs)
	{
		// info about accel structure
		vk::AccelerationStructureInfoNV as_info = {};
		{
			as_info.setType(vk::AccelerationStructureTypeNV::eBottomLevel);
			as_info.setGeometryCount(1);
			as_info.setPGeometries(&vk_geometry_nvs);
			as_info.setFlags(vk::BuildAccelerationStructureFlagBitsNV::ePreferFastTrace);
		}
		vk::AccelerationStructureCreateInfoNV as_ci = {};
		{
			as_ci.setInfo(as_info);
		}

		// create accel
		vk::UniqueAccelerationStructureNV accel = Core::Inst().m_vk_device->createAccelerationStructureNVUnique(as_ci);
		vk::UniqueDeviceMemory memory = alloc_and_bind_memory(*accel);

		// find memory requirement for scratch buffer
		vk::AccelerationStructureMemoryRequirementsInfoNV mem_req_info;
		{
			mem_req_info.setType(vk::AccelerationStructureMemoryRequirementsTypeNV::eBuildScratch);
			mem_req_info.setAccelerationStructure(*accel);
		}
		vk::MemoryRequirements mem_req = (*Core::Inst().m_vk_device).getAccelerationStructureMemoryRequirementsNV(mem_req_info)
																	.memoryRequirements;

		// create scratch buffer
		Buffer scratch_buffer(mem_req.size,
							  vk::MemoryPropertyFlagBits::eDeviceLocal,
							  vk::BufferUsageFlagBits::eRayTracingNV);

		// build accel
		Core::Inst().one_time_command_submit(
			[&](vk::CommandBuffer & command_buffer)
			{
				command_buffer.buildAccelerationStructureNV(as_info,
															nullptr,
															0,
															false,
															*accel,
															nullptr,
															*scratch_buffer.m_vk_buffer,
															0);
				vk::MemoryBarrier barrier;
				{
					barrier.setSrcAccessMask(vk::AccessFlagBits::eAccelerationStructureReadNV);
					barrier.setDstAccessMask(vk::AccessFlagBits::eAccelerationStructureWriteNV);
				}
				command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eAccelerationStructureBuildNV,
											   vk::PipelineStageFlagBits::eAccelerationStructureBuildNV,
											   vk::DependencyFlags(),
											   { barrier },
											   nullptr,
											   nullptr);
			});

		return { std::move(accel), std::move(memory) };
	}

	vk::UniqueDeviceMemory
	alloc_and_bind_memory(const vk::AccelerationStructureNV & vk_accel_struct)
	{
		// find memory requirement
		vk::AccelerationStructureMemoryRequirementsInfoNV mem_req_info;
		{
			mem_req_info.setAccelerationStructure(vk_accel_struct);
		}
		vk::MemoryRequirements mem_req = (*Core::Inst().m_vk_device).getAccelerationStructureMemoryRequirementsNV(mem_req_info)
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
		vk::BindAccelerationStructureMemoryInfoNV bind_accel_mem;
		{
			bind_accel_mem.setAccelerationStructure(vk_accel_struct);
			bind_accel_mem.setMemory(*memory);
			bind_accel_mem.setMemoryOffset(0);
		}
		Core::Inst().m_vk_device->bindAccelerationStructureMemoryNV(bind_accel_mem);

		return memory;
	}
};


// Ray Tracing - top level accel struct (for triangles of a single mesh)
struct RtTlas
{
	vk::UniqueAccelerationStructureNV	m_vk_accel_struct;
	vk::UniqueDeviceMemory				m_vk_memory;

	RtTlas()
	{
	}

	RtTlas(const std::vector<RtBlas *> & rt_blases)
	{
		std::vector<vk::AccelerationStructureNV> blases(rt_blases.size());
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
		vk::UniqueAccelerationStructureNV	m_vk_accel_struct;
		vk::UniqueDeviceMemory				m_vk_memory;
	};
	ReturnType_create_tlas
	create_tlas(const std::vector<vk::AccelerationStructureNV> & instances)
	{
		// create empty tlas
		vk::Device & vk_device = *Core::Inst().m_vk_device;
		vk::AccelerationStructureInfoNV	as_info;
		{
			as_info.setType(vk::AccelerationStructureTypeNV::eTopLevel);
			as_info.setInstanceCount(uint32_t(instances.size()));
			as_info.setFlags(vk::BuildAccelerationStructureFlagBitsNV::ePreferFastTrace);
		}
		vk::AccelerationStructureCreateInfoNV as_ci;
		{
			as_ci.setCompactedSize(0);
			as_ci.setInfo(as_info);
		}
		vk::UniqueAccelerationStructureNV tlas = vk_device.createAccelerationStructureNVUnique(as_ci);
		vk::UniqueDeviceMemory memory = alloc_and_bind_memory(*tlas);

		// find out scratch buffer size
		vk::AccelerationStructureMemoryRequirementsInfoNV mem_requirements_info;
		{
			mem_requirements_info.setType(vk::AccelerationStructureMemoryRequirementsTypeNV::eBuildScratch);
			mem_requirements_info.setAccelerationStructure(*tlas);
		}
		vk::DeviceSize scratch_size = vk_device.getAccelerationStructureMemoryRequirementsNV(mem_requirements_info)
											   .memoryRequirements.size;

		// create scratch buffer
		Buffer scratch_buffer(scratch_size,
							  vk::MemoryPropertyFlagBits::eDeviceLocal,
							  vk::BufferUsageFlagBits::eRayTracingNV);

		// build instance descriptor (pointer to geometry) for each instance
		std::vector<VkGeometryInstanceNV> geometry_instances;
		geometry_instances.reserve(instances.size());
		for (size_t i_instance = 0; i_instance < instances.size(); i_instance++)
		{
			uint64_t as_handle = 0;
			vk_device.getAccelerationStructureHandleNV(instances[i_instance],
													   sizeof(as_handle),
													   &as_handle);

			// TODO:: transform!
			mat3x4 transform = glm::identity<mat3x4>();

			VkGeometryInstanceNV instance_nv;
			{
				instance_nv.accelerationStructureHandle = as_handle;
				instance_nv.flags = uint32_t(vk::GeometryInstanceFlagBitsNV::eTriangleCullDisable);
				instance_nv.hitGroupId = 0;
				instance_nv.instanceId = uint32_t(i_instance);
				instance_nv.mask = 0xff;
				std::memcpy(instance_nv.transform,
							&transform[0][0],
							sizeof(mat3x4));
			}
			geometry_instances.push_back(instance_nv);
		}

		// transfer the instance descriptors to GPU via buffer
		Buffer instance_buffer(geometry_instances,
							   vk::MemoryPropertyFlagBits::eDeviceLocal,
							   vk::BufferUsageFlagBits::eRayTracingNV);

		// build tlas
		Core::Inst().one_time_command_submit(
			[&](vk::CommandBuffer & command_buffer)
			{
				// make sure that instance_buffer is completely written before we proceed further.
				vk::MemoryBarrier barrier(vk::AccessFlagBits::eTransferWrite,
										  vk::AccessFlagBits::eAccelerationStructureWriteNV);
				command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
											   vk::PipelineStageFlagBits::eAccelerationStructureBuildNV,
											   vk::DependencyFlags(),
											   { barrier },
											   {},
											   {});
				
				// build tlas
				command_buffer.buildAccelerationStructureNV(as_info,
															*instance_buffer.m_vk_buffer,
															0,
															false,
															*tlas,
															nullptr,
															*scratch_buffer.m_vk_buffer,
															0);
			});

		return { std::move(tlas), std::move(memory) };
	}

	vk::UniqueDeviceMemory
	alloc_and_bind_memory(const vk::AccelerationStructureNV & vk_accel_struct)
	{
		// find memory requirement
		vk::AccelerationStructureMemoryRequirementsInfoNV mem_req_info;
		{
			mem_req_info.setAccelerationStructure(vk_accel_struct);
		}
		vk::MemoryRequirements mem_req = (*Core::Inst().m_vk_device).getAccelerationStructureMemoryRequirementsNV(mem_req_info)
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
		vk::BindAccelerationStructureMemoryInfoNV bind_accel_mem;
		{
			bind_accel_mem.setAccelerationStructure(vk_accel_struct);
			bind_accel_mem.setMemory(*memory);
			bind_accel_mem.setMemoryOffset(0);
		}
		Core::Inst().m_vk_device->bindAccelerationStructureMemoryNV(bind_accel_mem);

		return memory;
	}
};