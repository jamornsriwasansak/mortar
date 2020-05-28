#pragma once

#include "common/mortar.h"
#include "common/scene.h"
#include "common/camera.h"
#include "common/stopwatch.h"

#include "gavulkan/appcore.h"
#include "gavulkan/image.h"
#include "gavulkan/buffer.h"
#include "gavulkan/shader.h"
#include "gavulkan/raytracingpipeline.h"
#include "gavulkan/graphicspipeline.h"

struct VoxelizerUboParams
{
	ivec3 m_voxel_resolution;
	uint m_num_instances;
	vec3 m_bbox_min;
	uint m_num_faces;
	vec3 m_bbox_max;
	float padding;
};

struct VoxelizedScene
{
	vec3 m_bbox_min;
	vec3 m_bbox_max;
	ivec3 m_resolution;
	std::vector<int> m_data;

	int &
	operator[](const ivec3 & index3)
	{
		const int index = index3.x
						  + m_resolution.x * index3.y
						  + m_resolution.x * m_resolution.y * index3.z;
		return m_data[index];
	}

	const int
	operator[](const ivec3 & index3) const
	{
		const int index = index3.x
						  + m_resolution.x * index3.y
						  + m_resolution.x * m_resolution.y * index3.z;
		return m_data[index];
	}

	vec3
	get_voxel_size() const
	{
		return (m_bbox_max - m_bbox_min) / vec3(m_resolution);
	}

	vec3
	get_position(const ivec3 & index3) const
	{
		vec3 mid = vec3(index3) + vec3(0.5f);
		vec3 offset = mid * (m_bbox_max - m_bbox_min) / vec3(m_resolution);
		return m_bbox_min + offset;
	}

	uint32_t
	flood_fill(const ivec3 & initial_filling_point,
			   const int value)
	{
		// number of floodfilled voxels
		uint32_t num_flood_filled_voxel = 0;

		// traversal stack
		std::vector<ivec3> stack;
		stack.push_back(initial_filling_point);

		// previous value marked at this initial filling point
		const int prev_value = (*this)[initial_filling_point];

		while (true)
		{
			// if there's no voxel left that we can walk from, stop the flood fill process
			if (stack.empty())
			{
				break;
			}

			// get element on top of the stack
			const ivec3 voxel_pos = stack.back();
			stack.pop_back();

			// if voxel position is out of bound, ignore
			if (any(greaterThanEqual(voxel_pos, m_resolution)))
			{
				continue;
			}
			if (any(lessThan(voxel_pos, ivec3(0))))
			{
				continue;
			}

			// if voxel position does not share the same value as the floodfill initial point, ignore
			if ((*this)[voxel_pos] != prev_value)
			{
				continue;
			}

			// if all tested has passed, mark visited and push neighbours
			(*this)[voxel_pos] = value;
			stack.push_back(voxel_pos + ivec3(0, 0, -1));
			stack.push_back(voxel_pos + ivec3(0, 0, +1));
			stack.push_back(voxel_pos + ivec3(0, -1, 0));
			stack.push_back(voxel_pos + ivec3(0, +1, 0));
			stack.push_back(voxel_pos + ivec3(-1, 0, 0));
			stack.push_back(voxel_pos + ivec3(+1, 0, 0));

			num_flood_filled_voxel++;
		}
		return num_flood_filled_voxel;
	}

	void
	write_obj(const std::filesystem::path & path,
			  const int flag = 1) const
	{
		std::ofstream ofs(path);
		for (int z = 0; z < m_resolution.z; z++)
			for (int y = 0; y < m_resolution.y; y++)
				for (int x = 0; x < m_resolution.x; x++)
				{
					int index = x + m_resolution.x * y + m_resolution.x * m_resolution.y * z;
					if (m_data[index] == flag)
					{
						const vec3 position = get_position(ivec3(x, y, z));
						ofs << "v " << position.x << " " << position.y << " " << position.z << std::endl;
					}
				}
	}
};

// voxelize on GPU, make solid on CPU (if make_solid is requested)
struct Voxelizer
{
	std::vector<int>
	convert_to_int(const std::vector<vec4> & voxel_info)
	{
		std::vector<int> result(voxel_info.size());
		for (size_t i = 0; i < voxel_info.size(); i++)
		{
			if (length(voxel_info.at(i)) >= SMALL_VALUE)
			{
				result.at(i) = 1;
			}
			else
			{
				result.at(i) = 0;
			}
		}
		return result;
	}

	int
	flood_fill(std::vector<int> * floodfilled,
			   const ivec3 & initial_filling_point,
			   const ivec3 & resolution,
			   const int value)
	{
	}

	VoxelizedScene
	run(const Scene & scene,
		const int min_voxel_resolution,
		const float expanding_factor,
		const bool make_solid = false)
	{
		THROW_ASSERT(expanding_factor > 1.0f, "expect expanding factor to be more than 1.0");

		const uint32_t num_instances = static_cast<uint32_t>(scene.m_triangle_instances.size());

		/*
		* shader
		*/
		Shader compute_shader("shaders/compute/voxelizer/voxelizer.comp",
							  vk::ShaderStageFlagBits::eCompute);
		compute_shader.m_uniforms_set.at({ 0, 2 })->m_num_descriptors = num_instances;
		compute_shader.m_uniforms_set.at({ 0, 3 })->m_num_descriptors = num_instances;


		/*
		* create compute pipeline
		*/

		const std::vector<const Shader *> shaders = { &compute_shader };
		DescriptorManager m_descriptor_manager(shaders);
		std::vector<vk::DescriptorSetLayout> vk_descriptor_set_layouts = m_descriptor_manager.get_vk_descriptor_set_layouts();

		// create pipeline cache
		vk::PipelineCacheCreateInfo pipeline_cache_ci = {};
		vk::UniquePipelineCache pipeline_cache = Core::Inst().m_vk_device->createPipelineCacheUnique(pipeline_cache_ci);

		// pipeline layout
		vk::PipelineLayoutCreateInfo layout_ci = {};
		{
			layout_ci.setSetLayoutCount(static_cast<uint32_t>(vk_descriptor_set_layouts.size()));
			layout_ci.setPSetLayouts(vk_descriptor_set_layouts.data());
		}
		vk::UniquePipelineLayout m_pipeline_layout = Core::Inst().m_vk_device->createPipelineLayoutUnique(layout_ci);

		// creating compute pipeline
		vk::ComputePipelineCreateInfo compute_pipeline_ci = {};
		{
			compute_pipeline_ci.setLayout(*m_pipeline_layout);
			compute_pipeline_ci.setStage(compute_shader.get_pipeline_shader_stage_ci());
		}
		vk::UniquePipeline compute_pipeline = Core::Inst().m_vk_device->createComputePipelineUnique(*pipeline_cache,
																									compute_pipeline_ci);


		/*
		* create necessary resource
		*/

		// find appropriate bounding box, assure that min_extent_len matches the given resolution
		const vec3 bbox_len = scene.m_bbox_max - scene.m_bbox_min;
		float min_extent_len = std::numeric_limits<float>::infinity();
		for (glm::length_t i = 0; i < 3; i++)
		{
			min_extent_len = std::min(bbox_len[i], min_extent_len);
		}
		const float voxel_size = min_extent_len / static_cast<float>(min_voxel_resolution);
		ivec3 voxel_resolution;
		for (glm::length_t i = 0; i < 3; i++)
		{
			const float voxel_res = std::ceil(bbox_len[i] / voxel_size);
			voxel_resolution[i] = int(voxel_res);
		}
		vec3 new_bbox_min = scene.m_bbox_min;
		vec3 new_bbox_max = new_bbox_min + voxel_size * vec3(voxel_resolution);
		
		// expand bounding box a bit (prevent numerical error in GPU voxelization, e.g. triangle that align with the xy plane perfectly)
		vec3 new_bbox_center = (new_bbox_min + new_bbox_max) * 0.5f;
		vec3 new_bbox_size = expanding_factor * (new_bbox_max - new_bbox_min);
		new_bbox_min = new_bbox_center - new_bbox_size * 0.5f;
		new_bbox_max = new_bbox_center + new_bbox_size * 0.5f;

		// create array of number of faces
		std::vector<int> num_faces_array;
		size_t num_total_faces = 0;
		for (size_t i = 0; i < scene.m_triangle_instances.size(); i++)
		{
			const size_t num_faces = scene.m_triangle_instances[i].m_triangle_mesh->m_num_triangles;
			num_total_faces += num_faces;
			num_faces_array.push_back(static_cast<int>(num_faces));
		}

		// create buffer of array of num_faces 
		Buffer num_faces_array_buffer(num_faces_array,
									  vk::MemoryPropertyFlagBits::eDeviceLocal,
									  vk::BufferUsageFlagBits::eStorageBuffer);

		// ubo params
		VoxelizerUboParams ubo;
		ubo.m_voxel_resolution = voxel_resolution;
		ubo.m_num_faces = static_cast<uint32_t>(num_total_faces);
		ubo.m_num_instances = static_cast<uint32_t>(scene.m_triangle_instances.size());
		ubo.m_bbox_min = new_bbox_min;
		ubo.m_bbox_max = new_bbox_max;
		Buffer voxel_ubo_params(sizeof(VoxelizerUboParams),
								vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible,
								vk::BufferUsageFlagBits::eUniformBuffer);
		voxel_ubo_params.copy_from(&ubo,
								   sizeof(ubo));

		// create storage image for raytracing
		RgbaImage<float> voxel_storage(voxel_resolution,
									   vk::ImageUsageFlagBits::eStorage,
									   true);
		voxel_storage.init_sampler();


		/*
		* descriptor sets
		*/

		std::vector<vk::DescriptorSet> descriptor_sets_0 = m_descriptor_manager
			.make_descriptor_sets_builder(0)
			.set_uniform_buffer(0, voxel_ubo_params)
			.set_storage_image(1, voxel_storage)
			.set_storage_buffers_array(2, scene.get_indices_arrays_storage())
			.set_storage_buffers_array(3, scene.get_position_u_storage())
			.set_storage_buffer(4, num_faces_array_buffer)
			.build();


		/*
		* create command buffers to execute the compute shader ONCE
		*/

		vk::UniqueCommandBuffer cmd_buffer = Core::Inst().create_command_buffer(vk::QueueFlagBits::eCompute);

		vk::CommandBufferBeginInfo command_buffer_begin_info = {};
		{
			command_buffer_begin_info.setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse);
			command_buffer_begin_info.setPInheritanceInfo(nullptr);
		}

		// begin
		cmd_buffer->begin(command_buffer_begin_info);
		cmd_buffer->bindPipeline(vk::PipelineBindPoint::eCompute,
								 *compute_pipeline);
		cmd_buffer->bindDescriptorSets(vk::PipelineBindPoint::eCompute,
									   *m_pipeline_layout,
									   0,
									   {
										   descriptor_sets_0
									   },
									   { });
		cmd_buffer->dispatch(static_cast<uint32_t>(num_total_faces), 1u, 1u);
		cmd_buffer->end();

		vk::SubmitInfo submit_info = { };
		{
			submit_info.setCommandBufferCount(1);
			submit_info.setPCommandBuffers(&cmd_buffer.get());
		}

		// submit
		Core::Inst().m_vk_compute_queue.submit({ submit_info }, nullptr);
		Core::Inst().m_vk_compute_queue.waitIdle();

		/*
		* floodfilled
		*/
		std::vector<vec4> voxels_raw = voxel_storage.download4();
		std::vector<int> voxels = convert_to_int(voxels_raw);

		VoxelizedScene result;
		result.m_bbox_min = new_bbox_min;
		result.m_bbox_max = new_bbox_max;
		result.m_data = std::move(voxels);
		result.m_resolution = voxel_resolution;

		return result;
	}
};
