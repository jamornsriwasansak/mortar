#pragma once

#include "common/mortar.h"
#include "common/trimesh.h"
#include "common/camera.h"

#include "gavulkan/appcore.h"
#include "gavulkan/raytracingaccel.h"

struct Scene
{
	std::vector<TriangleMeshInstance>	m_triangle_instances;
	std::shared_ptr<ImageStorage>		m_images_cache;
	TriangleMeshStorage					m_triangle_meshes_storage;
	RtTlas								m_tlas;

	vec3								m_bbox_min;
	vec3								m_bbox_max;

	Scene():
		m_bbox_max(vec3(std::numeric_limits<float>::lowest())),
		m_bbox_min(vec3(std::numeric_limits<float>::max())),
		m_images_cache(std::make_shared<ImageStorage>()),
		m_triangle_meshes_storage()
	{
		m_triangle_meshes_storage.m_image_storage = m_images_cache;
	}

	void
	add_mesh(const std::filesystem::path & path,
			 const bool do_create_blas = false)
	{
		std::vector<TriangleMesh *> triangle_meshes = m_triangle_meshes_storage.add_obj_mesh(path,
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
		// build tlas
		std::vector<RtBlas *> rt_blases(m_triangle_instances.size());
		for (size_t i = 0; i < rt_blases.size(); i++)
		{
			rt_blases[i] = &m_triangle_instances[i].m_triangle_mesh->m_rt_blas;
		}
		m_tlas = RtTlas(rt_blases);

		// and build materials buffer
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

	Buffer *
	get_materials_buffer_storage() const
	{
		return m_triangle_meshes_storage.m_materials_buffer.get();
	}
};

