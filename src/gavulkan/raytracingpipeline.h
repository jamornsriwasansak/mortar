#pragma once

#include <vulkan/vulkan.hpp>

#include "gavulkan/attachment.h"
#include "gavulkan/buffer.h"
#include "gavulkan/descriptor.h"
#include "gavulkan/shader.h"

struct RayTracingPipeline
{
	vk::UniquePipelineLayout	m_vk_pipeline_layout;
	vk::UniquePipeline			m_vk_pipeline;
	DescriptorManager			m_descriptor_manager;
	Buffer						m_sbt_buffer;
	uint32_t					m_raygen_offset;
	uint32_t					m_miss_offset;
	uint32_t					m_hit_offset;
	uint32_t					m_stride;

	RayTracingPipeline(const std::vector<const Shader *> & unsorted_shaders):
		m_descriptor_manager(unsorted_shaders)
	{
		// first of all make sure that the shaders are sorted by types
		// raygen -> miss -> hit
		// easier for SBT format requirement
		auto rt_shaders = sort_by_shader_type(unsorted_shaders);
		auto & shaders = rt_shaders.m_sorted_shaders;
		create_ray_tracing_pipeline(shaders);
		m_sbt_buffer = create_sbt_buffer(*m_vk_pipeline, uint32_t(shaders.size()));

		// compute all stride
		const uint32_t shader_group_handle_size = Core::Inst().m_vk_rt_properties.shaderGroupHandleSize;
		const uint32_t raygen_size = shader_group_handle_size * rt_shaders.m_num_raygen_shaders;
		const uint32_t miss_size = shader_group_handle_size * rt_shaders.m_num_miss_shaders;
		const uint32_t hit_size = shader_group_handle_size * rt_shaders.m_num_hit_shaders;

		// set all offset
		m_raygen_offset = 0;
		m_miss_offset = m_raygen_offset + raygen_size;
		m_hit_offset = m_miss_offset + miss_size;
		m_stride = shader_group_handle_size;
	}

	RayTracingPipeline(const Shader & raygen_shader,
					   const Shader & raychit_shader,
					   const Shader & raymiss_shader):
		m_descriptor_manager({ &raygen_shader, &raychit_shader, &raymiss_shader })
	{
		create_ray_tracing_pipeline({ &raygen_shader,
								 &raychit_shader,
								 &raymiss_shader });
		m_sbt_buffer = create_sbt_buffer(*m_vk_pipeline, uint32_t(3));
	}

	void
	create_ray_tracing_pipeline(const std::vector<const Shader *> & shaders)
	{
		auto & device = Core::Inst().m_vk_device;

		std::vector<vk::DescriptorSetLayout> descriptor_set_layouts = m_descriptor_manager.get_vk_descriptor_set_layouts();

		vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
		{
			pipelineLayoutCreateInfo.setLayoutCount = uint32_t(descriptor_set_layouts.size());
			pipelineLayoutCreateInfo.pSetLayouts = descriptor_set_layouts.data();
			if (m_descriptor_manager.m_vk_push_constant_range.has_value())
			{
				pipelineLayoutCreateInfo.pPushConstantRanges = &m_descriptor_manager.m_vk_push_constant_range.value();
				pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
			}
		}

		m_vk_pipeline_layout = device->createPipelineLayoutUnique(pipelineLayoutCreateInfo);

		std::vector<vk::PipelineShaderStageCreateInfo> shader_stages;
		std::vector<vk::RayTracingShaderGroupCreateInfoNV> shader_groups;
		for (size_t i_shader = 0; i_shader < shaders.size(); i_shader++)
		{
			vk::PipelineShaderStageCreateInfo shader_stage = shaders[i_shader]->get_pipeline_shader_stage_ci();
			vk::RayTracingShaderGroupCreateInfoNV shader_group = shaders[i_shader]->get_raytracing_shader_group_ci(i_shader);
			shader_stages.push_back(shader_stage);
			shader_groups.push_back(shader_group);
		}

		vk::RayTracingPipelineCreateInfoNV rayPipelineInfo{};
		{
			rayPipelineInfo.setStageCount(uint32_t(shader_stages.size()));
			rayPipelineInfo.setPStages(shader_stages.data());
			rayPipelineInfo.setGroupCount(uint32_t(shader_groups.size()));
			rayPipelineInfo.setPGroups(shader_groups.data());
			rayPipelineInfo.setMaxRecursionDepth(1);
			rayPipelineInfo.setLayout(*m_vk_pipeline_layout);
		}

		m_vk_pipeline = device->createRayTracingPipelineNVUnique(nullptr, rayPipelineInfo);
	}

	DescriptorSetsBuilder
	build_descriptor_sets(const size_t i_set) const
	{
		return m_descriptor_manager.make_descriptor_sets_builder(i_set);
	}

	vk::Buffer
	sbt_vk_buffer()
	{
		return *m_sbt_buffer.m_vk_buffer;
	}

private:

	struct ReturnType_sort_by_shader_type
	{
		std::vector<const Shader *> m_sorted_shaders;
		uint32_t m_num_raygen_shaders = 0;
		uint32_t m_num_miss_shaders = 0;
		uint32_t m_num_hit_shaders = 0;
	};
	ReturnType_sort_by_shader_type
	sort_by_shader_type(const std::vector<const Shader *> & shaders)
	{
		uint32_t num_raygen_shaders = 0;
		uint32_t num_miss_shaders = 0;
		uint32_t num_hit_shaders = 0;

		std::vector<const Shader *> sorted_shaders;
		for (size_t i_shader = 0; i_shader < shaders.size(); i_shader++)
		{
			if (shaders[i_shader]->m_vk_shader_stage & vk::ShaderStageFlagBits::eRaygenNV)
			{
				sorted_shaders.push_back(shaders[i_shader]);
				num_raygen_shaders++;
			}
		}

		for (size_t i_shader = 0; i_shader < shaders.size(); i_shader++)
		{
			if (shaders[i_shader]->m_vk_shader_stage & vk::ShaderStageFlagBits::eMissNV)
			{
				sorted_shaders.push_back(shaders[i_shader]);
				num_miss_shaders++;
			}
		}

		for (size_t i_shader = 0; i_shader < shaders.size(); i_shader++)
		{
			if (shaders[i_shader]->m_vk_shader_stage & (vk::ShaderStageFlagBits::eAnyHitNV | vk::ShaderStageFlagBits::eClosestHitNV))
			{
				sorted_shaders.push_back(shaders[i_shader]);
				num_hit_shaders++;
			}
		}

		ReturnType_sort_by_shader_type result;
		{
			result.m_sorted_shaders = sorted_shaders;
			result.m_num_raygen_shaders = num_raygen_shaders;
			result.m_num_miss_shaders = num_miss_shaders;
			result.m_num_hit_shaders = num_hit_shaders;
		}

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
		Core::Inst().m_vk_device->getRayTracingShaderGroupHandlesNV(rt_pipeline,
																	0,
																	num_shader_groups,
																	sbt_size,
																	shader_handle_storage.data());

		// create buffer
		Buffer sbt_buffer(shader_handle_storage,
						  vk::MemoryPropertyFlagBits::eDeviceLocal,
						  vk::BufferUsageFlagBits::eRayTracingNV);

		return sbt_buffer;
	}
};
