#pragma once

#include <vulkan/vulkan.hpp>

#include "gavulkan/attachment.h"
#include "gavulkan/buffer.h"
#include "gavulkan/descriptor.h"
#include "gavulkan/shader.h"

struct RayTracingPipeline
{
	vk::UniquePipelineLayout	m_vk_pipeline_layout;
	vk::Pipeline				m_vk_pipeline;
	DescriptorManager			m_descriptor_manager;
	Buffer						m_sbt_buffer;
	vk::StridedBufferRegionKHR	m_sbt_raygen;
	vk::StridedBufferRegionKHR	m_sbt_miss;
	vk::StridedBufferRegionKHR	m_sbt_hit;
	vk::StridedBufferRegionKHR	m_sbt_callable;

	RayTracingPipeline(const std::vector<const Shader *> & unsorted_shaders,
					   const uint32_t recursion_depth = 1):
		m_descriptor_manager(unsorted_shaders)
	{
		// first of all make sure that the shaders are sorted by types
		// raygen -> miss -> hit
		// easier for SBT format requirement
		auto rt_shaders = sort_by_shader_type(unsorted_shaders);
		const uint32_t num_shader_groups = create_ray_tracing_pipeline(rt_shaders.m_raygen_shader,
																	   rt_shaders.m_miss_shaders,
																	   rt_shaders.m_closest_hit_shader,
																	   rt_shaders.m_any_hit_shader,
																	   recursion_depth);

		// count hit shaders
		uint32_t num_hit_shaders = 0;
		num_hit_shaders += rt_shaders.m_any_hit_shader == nullptr ? 1 : 0;
		num_hit_shaders += rt_shaders.m_closest_hit_shader == nullptr ? 1 : 0;

		// compute all size
		const uint32_t shader_group_handle_size = Core::Inst().m_vk_rt_properties.shaderGroupHandleSize;
		const uint32_t raygen_size = shader_group_handle_size;
		const uint32_t miss_size = shader_group_handle_size * static_cast<uint32_t>(rt_shaders.m_miss_shaders.size());
		const uint32_t hit_size = shader_group_handle_size * num_hit_shaders;

		// set all offset
		vk::DeviceSize raygen_offset = 0;
		vk::DeviceSize miss_offset = raygen_offset + raygen_size;
		vk::DeviceSize hit_offset = miss_offset + miss_size;
		vk::DeviceSize callable_offset = hit_offset + hit_size;

		m_sbt_buffer = create_sbt_buffer(m_vk_pipeline, num_shader_groups);

		m_sbt_raygen = vk::StridedBufferRegionKHR()
			.setBuffer(*m_sbt_buffer.m_vk_buffer)
			.setOffset(raygen_offset)
			.setStride(m_sbt_buffer.m_element_size_in_bytes)
			.setSize(m_sbt_buffer.size_in_bytes());

		m_sbt_miss = vk::StridedBufferRegionKHR()
			.setBuffer(*m_sbt_buffer.m_vk_buffer)
			.setOffset(miss_offset)
			.setStride(m_sbt_buffer.m_element_size_in_bytes)
			.setSize(m_sbt_buffer.size_in_bytes());

		m_sbt_hit = vk::StridedBufferRegionKHR()
			.setBuffer(*m_sbt_buffer.m_vk_buffer)
			.setOffset(hit_offset)
			.setStride(m_sbt_buffer.m_element_size_in_bytes)
			.setSize(m_sbt_buffer.size_in_bytes());

		m_sbt_callable = vk::StridedBufferRegionKHR()
			.setBuffer(*m_sbt_buffer.m_vk_buffer)
			.setOffset(callable_offset)
			.setStride(0)
			.setSize(m_sbt_buffer.size_in_bytes());
	}

	uint32_t
	create_ray_tracing_pipeline(const Shader * raygen_shader,
								std::vector<const Shader *> & miss_shaders,
								const Shader * closest_hit_shader,
								const Shader * any_hit_shader,
								const uint32_t recursion_depth)
	{
		auto & device = Core::Inst().m_vk_device;

		std::vector<vk::DescriptorSetLayout> descriptor_set_layouts = m_descriptor_manager.get_vk_descriptor_set_layouts();

		vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
		{
			pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(descriptor_set_layouts.size());
			pipelineLayoutCreateInfo.pSetLayouts = descriptor_set_layouts.data();
			if (m_descriptor_manager.m_vk_push_constant_range.has_value())
			{
				pipelineLayoutCreateInfo.pPushConstantRanges = &m_descriptor_manager.m_vk_push_constant_range.value();
				pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
			}
		}

		m_vk_pipeline_layout = device->createPipelineLayoutUnique(pipelineLayoutCreateInfo);

		std::vector<vk::PipelineShaderStageCreateInfo> shader_stages;
		std::vector<vk::RayTracingShaderGroupCreateInfoKHR> shader_groups;

		// raygen
		vk::RayTracingShaderGroupCreateInfoKHR raygen_shader_group = raygen_shader->get_raytracing_shader_group_ci(static_cast<uint32_t>(shader_stages.size()));
		vk::PipelineShaderStageCreateInfo raygen_shader_stage = raygen_shader->get_pipeline_shader_stage_ci();
		shader_stages.push_back(raygen_shader_stage);
		shader_groups.push_back(raygen_shader_group);

		// miss shaders
		for (const Shader * miss_shader : miss_shaders)
		{
			vk::RayTracingShaderGroupCreateInfoKHR miss_shader_group = miss_shader->get_raytracing_shader_group_ci(static_cast<uint32_t>(shader_stages.size()));
			vk::PipelineShaderStageCreateInfo miss_shader_stage = miss_shader->get_pipeline_shader_stage_ci();
			shader_stages.push_back(miss_shader_stage);
			shader_groups.push_back(miss_shader_group);
		}

		// hit shader
		vk::RayTracingShaderGroupCreateInfoKHR hit_shader_group;
		hit_shader_group.setType(vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup);
		hit_shader_group.setAnyHitShader(VK_SHADER_UNUSED_KHR);
		hit_shader_group.setClosestHitShader(VK_SHADER_UNUSED_KHR);
		hit_shader_group.setGeneralShader(VK_SHADER_UNUSED_KHR);
		hit_shader_group.setIntersectionShader(VK_SHADER_UNUSED_KHR);
		if (closest_hit_shader)
		{
			vk::PipelineShaderStageCreateInfo closest_hit_shader_stage = closest_hit_shader->get_pipeline_shader_stage_ci();
			hit_shader_group.setClosestHitShader(static_cast<uint32_t>(shader_stages.size()));
			shader_stages.push_back(closest_hit_shader_stage);
		}
		if (any_hit_shader)
		{
			vk::PipelineShaderStageCreateInfo any_hit_shader_stage = any_hit_shader->get_pipeline_shader_stage_ci();
			hit_shader_group.setAnyHitShader(static_cast<uint32_t>(shader_stages.size()));
			shader_stages.push_back(any_hit_shader_stage);
		}
		shader_groups.push_back(hit_shader_group);

		// create ray tracing pipeline
		vk::RayTracingPipelineCreateInfoKHR ray_pipeline_ci = {};
		{
			ray_pipeline_ci.setStageCount(static_cast<uint32_t>(shader_stages.size()));
			ray_pipeline_ci.setPStages(shader_stages.data());
			ray_pipeline_ci.setGroupCount(static_cast<uint32_t>(shader_groups.size()));
			ray_pipeline_ci.setPGroups(shader_groups.data());
			ray_pipeline_ci.setMaxRecursionDepth(recursion_depth);
			ray_pipeline_ci.setLayout(*m_vk_pipeline_layout);
		}
		m_vk_pipeline = device->createRayTracingPipelineKHR(nullptr, ray_pipeline_ci).value;

		return static_cast<uint32_t>(shader_groups.size());
	}

	DescriptorSetsBuilder
	build_descriptor_sets(const size_t i_set) const
	{
		const size_t num_swapchain_images = Core::Inst().m_vk_swapchain_images.size();
		return m_descriptor_manager.make_descriptor_sets_builder(i_set,
																 num_swapchain_images);
	}

	vk::Buffer
	sbt_vk_buffer()
	{
		return *m_sbt_buffer.m_vk_buffer;
	}

private:

	struct ReturnType_sort_by_shader_type
	{
		const Shader * m_raygen_shader = nullptr;
		std::vector<const Shader *> m_miss_shaders;
		const Shader * m_closest_hit_shader = nullptr;
		const Shader * m_any_hit_shader = nullptr;
	};
	ReturnType_sort_by_shader_type
	sort_by_shader_type(const std::vector<const Shader *> & shaders)
	{
		uint32_t num_raygen_shaders = 0;
		uint32_t num_miss_shaders = 0;
		uint32_t num_any_hit_shader = 0;
		uint32_t num_closest_hit_shader = 0;
		ReturnType_sort_by_shader_type result;
		for (const Shader * shader : shaders)
		{
			if (shader->m_vk_shader_stage & vk::ShaderStageFlagBits::eRaygenKHR)
			{
				num_raygen_shaders++;
				result.m_raygen_shader = shader;
			}

			if (shader->m_vk_shader_stage & vk::ShaderStageFlagBits::eMissKHR)
			{
				num_miss_shaders++;
				result.m_miss_shaders.push_back(shader);
			}

			if (shader->m_vk_shader_stage & vk::ShaderStageFlagBits::eAnyHitKHR)
			{
				num_any_hit_shader++;
				result.m_any_hit_shader = shader;
			}

			if (shader->m_vk_shader_stage & vk::ShaderStageFlagBits::eClosestHitKHR)
			{
				num_closest_hit_shader++;
				result.m_closest_hit_shader = shader;
			}
		}
		THROW_ASSERT(num_raygen_shaders == 1,
					 "only one raygen shader per pipeline is expected");
		THROW_ASSERT(!(num_any_hit_shader == 0 && num_closest_hit_shader == 0),
					 "atleast one hit shader per pipeline is expected");
		return result;
	}

	Buffer
	create_sbt_buffer(const vk::Pipeline rt_pipeline,
					  const uint32_t num_shader_groups)
	{
		const uint32_t shader_group_handle_size = Core::Inst().m_vk_rt_properties.shaderGroupHandleSize;
		const uint32_t sbt_size = num_shader_groups * shader_group_handle_size;

		// create shader binding table
		std::vector<uint8_t> shader_handle_storage(sbt_size);
		Core::Inst().m_vk_device->getRayTracingShaderGroupHandlesKHR(rt_pipeline,
																	 0,
																	 num_shader_groups,
																	 sbt_size,
																	 shader_handle_storage.data());

		// create buffer
		Buffer sbt_buffer(shader_handle_storage.data(),
						  Core::Inst().m_vk_rt_properties.shaderGroupHandleSize,
						  num_shader_groups,
						  vk::MemoryPropertyFlagBits::eDeviceLocal,
						  vk::BufferUsageFlagBits::eRayTracingKHR);

		return sbt_buffer;
	}
};
