#pragma once

#include "common/mortar.h"
#include "gavulkan/appcore.h"
#include <vulkan/vulkan.hpp>

enum VertexBufferEnum { VertexBuffer };
enum UniformBufferEnum { UniformBuffer };
enum IndexBufferEnum { IndexBuffer };
enum RayTracingBufferEnum { RayTracingBuffer };

struct Buffer
{
	vk::UniqueBuffer			m_vk_buffer;
	vk::UniqueDeviceMemory		m_vk_memory;
	vk::BufferUsageFlags		m_vk_buffer_usage_flags;
	vk::Format					m_vk_format;
	size_t						m_element_size_in_bytes;
	size_t						m_num_elements;

	Buffer():
		m_vk_buffer(nullptr),
		m_vk_memory(nullptr),
		m_vk_buffer_usage_flags(),
		m_element_size_in_bytes(0),
		m_num_elements(0)
	{
	}

	Buffer(const void * data,
		   const size_t element_size_in_bytes,
		   const size_t num_elements,
		   const vk::MemoryPropertyFlags & mem_prop_flags,
		   const vk::BufferUsageFlags & buffer_usage_flags,
		   const vk::Format & format = vk::Format()):
		m_vk_buffer(nullptr),
		m_vk_memory(nullptr),
		m_vk_buffer_usage_flags(buffer_usage_flags),
		m_element_size_in_bytes(element_size_in_bytes),
		m_num_elements(num_elements),
		m_vk_format(format)
	{
		if (mem_prop_flags & vk::MemoryPropertyFlagBits::eDeviceLocal)
		{
			m_vk_buffer_usage_flags |= vk::BufferUsageFlagBits::eTransferDst;
		}

		// create unique buffer
		vk::BufferCreateInfo vertex_buffer_ci = {};
		{
			vertex_buffer_ci.setSize(uint32_t(size_in_bytes()));
			vertex_buffer_ci.setUsage(m_vk_buffer_usage_flags);
			vertex_buffer_ci.setSharingMode(vk::SharingMode::eExclusive);
		}
		m_vk_buffer = Core::Inst().m_vk_device->createBufferUnique(vertex_buffer_ci);

		// get requirement
		vk::MemoryRequirements vertex_mem_requirements = Core::Inst().m_vk_device->getBufferMemoryRequirements(*m_vk_buffer);
		const uint32_t mem_type = find_mem_type(Core::Inst().m_vk_physical_device,
												vertex_mem_requirements.memoryTypeBits,
												mem_prop_flags);

		// alloc mem
		vk::MemoryAllocateInfo mem_ai = {};
		{
			mem_ai.setAllocationSize(vertex_mem_requirements.size);
			mem_ai.setMemoryTypeIndex(mem_type);
		}
		m_vk_memory = Core::Inst().m_vk_device->allocateMemoryUnique(mem_ai);

		// bind mem and buffer together
		const size_t mem_offset = 0;
		Core::Inst().m_vk_device->bindBufferMemory(*m_vk_buffer,
												   *m_vk_memory,
												   vk::DeviceSize(mem_offset));

		// copy if not null
		if (data != nullptr)
		{ 
			if (mem_prop_flags & vk::MemoryPropertyFlagBits::eDeviceLocal)
			{
				// create staging buffer
				Buffer stage(data,
							 element_size_in_bytes,
							 num_elements,
							 vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
							 vk::BufferUsageFlagBits::eTransferSrc);

				copy_from(stage);
			}
			else if (mem_prop_flags & vk::MemoryPropertyFlagBits::eHostVisible)
			{
				copy_from(data,
						  element_size_in_bytes * num_elements);
			}
			else
			{
				THROW("unknown supported property flag bits");
			}
		}
	}

	template<typename T>
	Buffer(const std::vector<T> data,
		   const vk::MemoryPropertyFlags & mem_prop_flags,
		   const vk::BufferUsageFlags & buffer_usage_flags,
		   const vk::Format & format = vk::Format()):
		Buffer(data.data(),
			   sizeof(T),
			   data.size(),
			   mem_prop_flags,
			   buffer_usage_flags,
			   format)
	{
	}

	Buffer(const size_t size_in_bytes,
		   const vk::MemoryPropertyFlags & mem_prop_flags,
		   const vk::BufferUsageFlags & buffer_usage_flags,
		   const vk::Format & format = vk::Format()):
		Buffer(nullptr,
			   size_in_bytes,
			   1,
			   mem_prop_flags,
			   buffer_usage_flags,
			   format)
	{
	}

	void
	copy_from(const void * data,
			  const size_t data_size_in_bytes)
	{
		THROW_ASSERT(size_in_bytes() == data_size_in_bytes,
					 "size mismatch the buffer size");

		// map, copy, unmap
		const vk::Device & device = *Core::Inst().m_vk_device;
		void * mapped_vertex_mem = device.mapMemory(*m_vk_memory,
													vk::DeviceSize(0),
													vk::DeviceSize(data_size_in_bytes),
													vk::MemoryMapFlagBits());
		std::memcpy(mapped_vertex_mem,
					data,
					data_size_in_bytes);
		device.unmapMemory(*m_vk_memory);
	}

	void
	copy_from(const Buffer & other)
	{
		THROW_ASSERT(other.m_num_elements == m_num_elements, "size_in_bytes must match");
		THROW_ASSERT(other.m_element_size_in_bytes == m_element_size_in_bytes, "element_size_in_bytes must match");

		// create command for copy buffer
		auto command_func =
			[&](vk::CommandBuffer& command_buffer)
			{
				// region of the buffer to copy
				vk::BufferCopy copy_region = {};
				{
					copy_region.setSize(vk::DeviceSize(m_element_size_in_bytes * m_num_elements));
					copy_region.setDstOffset(vk::DeviceSize(0));
					copy_region.setSrcOffset(vk::DeviceSize(0));
				}

				// record command into command buffer
				command_buffer.copyBuffer(*other.m_vk_buffer, *m_vk_buffer, copy_region);
			};

		// one time submit
		Core::Inst().one_time_command_submit(command_func);
	}

	size_t
	size_in_bytes() const
	{
		return m_element_size_in_bytes * m_num_elements;
	}

	vk::IndexType
	get_index_type() const
	{
		THROW_ASSERT(m_vk_buffer_usage_flags & vk::BufferUsageFlagBits::eIndexBuffer,
					 "get_index_type requested for a non index buffer");

		// try to interpret element_size whether it's uint8, uint16 or uint32
		size_t element_size_in_bytes = m_element_size_in_bytes;
		// if it's multiple of 3, we assume it has a type of ivec3/uvec3
		if (element_size_in_bytes % 3 == 0)
		{
			element_size_in_bytes = element_size_in_bytes / 3;
		}

		if (element_size_in_bytes == 1)
		{
			return vk::IndexType::eUint8EXT;
		}
		else if (element_size_in_bytes == 2)
		{
			return vk::IndexType::eUint16;
		}
		else if (element_size_in_bytes == 4)
		{
			return vk::IndexType::eUint32;
		}

		THROW_ASSERT(false,
					 "get_index_type could not interpret a buffer with an unknown element size");
		return vk::IndexType::eNoneNV;
	}

	size_t
	get_num_indices() const
	{
		THROW_ASSERT(m_vk_buffer_usage_flags & vk::BufferUsageFlagBits::eIndexBuffer,
					 "get_num_indices requested for a non index buffer");

		// try to interpret element_size whether it's uint8, uint16 or uint32
		size_t element_size_in_bytes = m_element_size_in_bytes;
		// if it's multiple of 3, we assume it has a type of ivec3/uvec3
		if (element_size_in_bytes % 3 == 0)
		{
			element_size_in_bytes = element_size_in_bytes / 3;
		}

		return m_num_elements;
	}
};