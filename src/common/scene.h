#pragma once

#include "common/mortar.h"
#include "common/trimesh.h"
#include "common/camera.h"
#include "common/envmap.h"

#include "gavulkan/appcore.h"
#include "gavulkan/raytracingaccel.h"

struct Scene
{
	std::vector<TriangleMeshInstance>	m_triangle_instances;
	std::shared_ptr<ImageStorage>		m_images_cache;
	TriangleMeshStorage					m_triangle_meshes_storage;
	RtTlas								m_tlas;
	std::unique_ptr<Buffer>				m_top_level_emitters_cdf = nullptr;
	std::unique_ptr<Buffer>				m_bottom_level_cdf_table_sizes = nullptr;
	Envmap								m_envmap;

	vec3								m_bbox_min;
	vec3								m_bbox_max;

	Scene():
		m_bbox_max(vec3(std::numeric_limits<float>::lowest())),
		m_bbox_min(vec3(std::numeric_limits<float>::max())),
		m_images_cache(std::make_shared<ImageStorage>()),
		m_triangle_meshes_storage(),
		m_envmap(true)
	{
		m_triangle_meshes_storage.m_image_storage = m_images_cache;
	}

	void
	add_mesh(const std::filesystem::path & path,
			 const bool do_create_blas = false)
	{
		std::vector<TriangleMesh *> triangle_meshes = m_triangle_meshes_storage.
			add_obj_mesh(path,
						 do_create_blas,
						 &m_bbox_min,
						 &m_bbox_max);
		for (size_t i = 0; i < triangle_meshes.size(); i++)
		{
			TriangleMeshInstance triangle_instance;
			triangle_instance.m_triangle_mesh = triangle_meshes[i];
			triangle_instance.m_transform_model = identity<mat4>();
			m_triangle_instances.push_back(triangle_instance);
		}
	}

	void
	build_rt()
	{
		/*
		* build tlas
		*/
		std::vector<RtBlas *> rt_blases(m_triangle_instances.size());
		for (size_t i = 0; i < rt_blases.size(); i++)
		{
			rt_blases[i] = &m_triangle_instances[i].m_triangle_mesh->m_rt_blas;
		}
		m_tlas = RtTlas(rt_blases);

		/*
		* build emitter support
		*/
		// all shapes are emitters but shapes without emissive values will have sampling probability of zero
		float sum = 0.0f;
		const size_t num_shapes = m_triangle_instances.size();
		const size_t num_envmap = 1;
		const size_t num_emitters = get_num_emitters();
		std::vector<float> top_level_pdf(num_emitters);
		std::vector<float> top_level_cdf(num_emitters + 1);
		std::vector<uint32_t> cdf_table_sizes(num_emitters);
		for (size_t i = 0; i < num_emitters; i++)
		{
			if (0 <= i && i < num_shapes)
			{
				cdf_table_sizes[i] = m_triangle_instances[i].m_triangle_mesh->m_cdf_buffer->m_num_elements;
				top_level_pdf[i] = m_triangle_instances[i].m_triangle_mesh->m_emission_weight;
			}
			else
			{
				cdf_table_sizes[i] = m_envmap.m_cdf_table.m_num_elements;
				top_level_pdf[i] = m_envmap.m_emissive_weight;
			}
			sum += top_level_pdf[i];
		}
		// normalize pdf
		for (size_t i = 0; i < num_emitters; i++)
		{
			top_level_pdf[i] /= sum;
		}
		// cdf
		top_level_cdf[0] = 0.0f;
		for (size_t i = 0; i < num_emitters; i++)
		{
			top_level_cdf[i + 1] = top_level_pdf[i] + top_level_cdf[i];
		}
		THROW_ASSERT(std::abs(top_level_cdf[num_emitters] - 1.0f) <= SMALL_VALUE,
					 "last element of cdf table is not 1.0f");
		top_level_cdf[num_emitters] = 1.0f;
		m_top_level_emitters_cdf = std::make_unique<Buffer>(top_level_cdf,
															vk::MemoryPropertyFlagBits::eDeviceLocal,
															vk::BufferUsageFlagBits::eRayTracingNV | vk::BufferUsageFlagBits::eStorageBuffer);
		m_bottom_level_cdf_table_sizes = std::make_unique<Buffer>(cdf_table_sizes,
																  vk::MemoryPropertyFlagBits::eDeviceLocal,
																  vk::BufferUsageFlagBits::eRayTracingNV | vk::BufferUsageFlagBits::eStorageBuffer);

		/*
		* and build materials buffer
		*/
		m_triangle_meshes_storage.build_materials_buffer();

		m_images_cache->generate_if_no_image();
	}

	std::vector<const Buffer *>
	get_indices_arrays_storage() const
	{
		std::vector<const Buffer *> buffers;
		for (size_t i = 0; i < m_triangle_instances.size(); i++)
		{
			buffers.push_back(m_triangle_instances[i].m_triangle_mesh->m_index_buffer.get());
		}
		return buffers;
	}

	std::vector<const Buffer *>
	get_position_u_storage() const
	{
		std::vector<const Buffer *> buffers;
		for (size_t i = 0; i < m_triangle_instances.size(); i++)
		{
			buffers.push_back(m_triangle_instances[i].m_triangle_mesh->m_position_and_u_buffer.get());
		}
		return buffers;
	}

	std::vector<const Buffer *>
	get_normal_v_storage() const
	{
		std::vector<const Buffer *> buffers;
		for (size_t i = 0; i < m_triangle_instances.size(); i++)
		{
			buffers.push_back(m_triangle_instances[i].m_triangle_mesh->m_normal_and_v_buffer.get());
		}
		return buffers;
	}

	std::vector<const Buffer *>
	get_material_id_storage() const
	{
		std::vector<const Buffer *> buffers;
		for (size_t i = 0; i < m_triangle_instances.size(); i++)
		{
			buffers.push_back(m_triangle_instances[i].m_triangle_mesh->m_material_id_buffer.get());
		}
		return buffers;
	}

	std::vector<const Buffer *>
	get_bottom_level_emitters_cdf() const
	{
		std::vector<const Buffer *> buffers;
		for (size_t i = 0; i < m_triangle_instances.size(); i++)
		{
			buffers.push_back(m_triangle_instances[i].m_triangle_mesh->m_cdf_buffer.get());
		}
		buffers.push_back(&m_envmap.m_cdf_table);
		return buffers;
	}

	Buffer *
	get_materials_buffer_storage() const
	{
		return m_triangle_meshes_storage.m_materials_buffer.get();
	}

	void
	set_envmap(const std::filesystem::path & path,
			   const bool build_cdf_table)
	{
		m_envmap = Envmap(path,
						  build_cdf_table);
	}

	size_t
	get_num_emitters()
	{
		const size_t num_shapes = m_triangle_instances.size();
		const size_t num_envmap = 1;
		const size_t num_emitters = num_shapes + num_envmap;
		return num_emitters;
	}
};
