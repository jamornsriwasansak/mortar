#pragma once

// include tinyobj
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include "common/mortar.h"

#include "gavulkan/image.h"
#include "gavulkan/buffer.h"
#include "gavulkan/raytracingaccel.h"

struct ImageStorage
{
	std::vector<std::pair<RgbaImage2d<uint8_t>, bool>>	m_images; // image and is_opaque flag
	std::map<std::filesystem::path, size_t>				m_image_index_from_path;

	std::vector<const RgbaImage2d<uint8_t> *>
	get_images()
	{
		std::vector<const RgbaImage2d<uint8_t> *> images;
		for (size_t i = 0; i < m_images.size(); i++)
		{
			images.push_back(&m_images[i].first);
		}
		return images;
	}

	// return index to image and whether it contains alpha channel or not
	std::pair<uint32_t, bool>
	fetch(const std::filesystem::path & path)
	{
		// if could find image return the number
		if (m_image_index_from_path.find(path) != m_image_index_from_path.end())
		{
			const size_t image_index = m_image_index_from_path.at(path);
			const bool is_opaque = m_images[image_index].second;
			return std::make_pair(uint32_t(image_index), is_opaque);
		}

		// check whether it has alpha channel or not
		const StbiUint8Result raw_image(path);
		bool is_opaque = true;
		for (size_t i = 3; i < raw_image.m_width * raw_image.m_height * 4; i += 4)
		{
			if (raw_image.m_stbi_data[i] != 255)
			{
				is_opaque = false;
				break;
			}
		}
		// else load the image
		m_images.emplace_back(std::move(RgbaImage2d<uint8_t>(path)),
							  is_opaque);

		// and return the index
		return std::make_pair(uint32_t(m_images.size() - 1), is_opaque);
	}

	void
	generate_if_no_image()
	{
		// fill in the array with some random image just to stop vulkan from popping up the error messages
		if (m_images.size() > 0) return;

		std::vector<uint8_t> blank_image(4, 0);
		m_images.emplace_back(std::move(RgbaImage2d<uint8_t>(blank_image.data(),
															 1,
															 1,
															 vk::ImageTiling::eOptimal,
															 RgbaImage2d<uint8_t>::RgbaUsageFlagBits)),
							  false);
		m_images[0].first.init_sampler();
	}
};

struct TriangleMesh
{
	std::shared_ptr<Buffer>		m_position_and_u_buffer;
	std::shared_ptr<Buffer>		m_normal_and_v_buffer;
	std::shared_ptr<Buffer>		m_index_buffer;
	std::shared_ptr<Buffer>		m_material_id_buffer;
	RtBlas						m_rt_blas;
};

struct TriangleMeshStorage
{
	std::vector<std::unique_ptr<TriangleMesh>>	m_triangle_meshes;
	std::vector<Material>						m_materials;
	std::shared_ptr<Buffer>						m_materials_buffer;
	std::shared_ptr<ImageStorage>				m_image_storage = nullptr;

	TriangleMeshStorage()
	{
	}

	TriangleMeshStorage(const std::shared_ptr<ImageStorage> & image_storage):
		m_image_storage(image_storage)
	{
	}

	void
	build_materials_buffer()
	{
		m_materials_buffer = std::make_shared<Buffer>(m_materials,
													  vk::MemoryPropertyFlagBits::eDeviceLocal,
													  vk::BufferUsageFlagBits::eRayTracingNV | vk::BufferUsageFlagBits::eStorageBuffer);
	}

	std::pair<vec3, bool>
	get_encoded_color(const float color[3],
					  const std::string & texname,
					  const std::filesystem::path & directory)
	{
		if (texname != "")
		{
			const auto tex_info = m_image_storage->fetch(directory / texname);
			return std::make_pair(vec3(-float(tex_info.first) - 1.0f), tex_info.second);
		}
		else
		{
			return std::make_pair(vec3(color[0], color[1], color[2]), true);
		}
	}

	std::pair<float, bool>
	get_encoded_color(const float color,
					  const std::string & texname,
					  const std::filesystem::path & directory)
	{
		if (texname != "")
		{
			const auto tex_info = m_image_storage->fetch(directory / texname);
			return std::make_pair(-float(tex_info.first) - 1.0f, tex_info.second);
		}
		else
		{
			return std::make_pair(color, false);
		}
	}

	std::vector<TriangleMesh *>
	add_obj_mesh(const std::filesystem::path & path,
				 const bool do_ray_tracing,
				 vec3 * bbox_min,
				 vec3 * bbox_max)
	{
		// make sure that the path is absolute
		const std::filesystem::path abs_path = std::filesystem::absolute(path);

		// load mesh using tinyobj loader
		tinyobj::attrib_t attrib;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> tmaterials;
		std::string warn;
		std::string err;
		tinyobj::LoadObj(&attrib,
						 &shapes,
						 &tmaterials,
						 &warn,
						 &err,
						 abs_path.string().c_str(),
						 abs_path.parent_path().string().c_str());

		if (!warn.empty())
		{
			THROW(warn);
		}

		if (!err.empty())
		{
			THROW(err);
		}

		/*
		* find a bbox size
		*/

		for (size_t i_vertex = 0; i_vertex < attrib.vertices.size() / 3; i_vertex++)
		{
			if (bbox_min)
			{
				bbox_min->x = std::min(attrib.vertices[i_vertex * 3 + 0], bbox_min->x);
				bbox_min->y = std::min(attrib.vertices[i_vertex * 3 + 1], bbox_min->y);
				bbox_min->z = std::min(attrib.vertices[i_vertex * 3 + 2], bbox_min->z);
			}

			if (bbox_max)
			{
				bbox_max->x = std::max(attrib.vertices[i_vertex * 3 + 0], bbox_max->x);
				bbox_max->y = std::max(attrib.vertices[i_vertex * 3 + 1], bbox_max->y);
				bbox_max->z = std::max(attrib.vertices[i_vertex * 3 + 2], bbox_max->z);
			}
		}

		/*
		* load all materials
		*/

		const uint32_t material_offset = uint32_t(m_materials.size());

		for (size_t i = 0; i < tmaterials.size(); i++)
		{
			const auto & tmat = tmaterials[i];

			Material material;
			const auto diffuse_refl = get_encoded_color(tmat.diffuse,
														tmat.diffuse_texname,
														path.parent_path());
			const auto spec_refl = get_encoded_color(tmat.specular,
													 tmat.specular_texname,
													 path.parent_path());
			const auto roughness = get_encoded_color(tmat.roughness,
													 tmat.roughness_texname,
													 path.parent_path());
			const auto emission = get_encoded_color(tmat.emission,
													tmat.emissive_texname,
													path.parent_path());
			material.m_diffuse_refl = diffuse_refl.first;
			material.m_spec_refl = spec_refl.first;
			material.m_roughness = roughness.first;
			material.m_emission = emission.first;
			const bool is_opaque = diffuse_refl.second;
			material.m_flags = is_opaque ? MATERIAL_FLAG_IS_OPAQUE : 0;
			m_materials.push_back(material);
		}

		/*
		* reorganize the data to make sure that vertex index = texcoord index = normal index
		*/

		// indices for all shape
		std::vector<std::vector<uint32_t>> shape_indices_arays;
		std::vector<std::vector<uint32_t>> shape_material_ids_arrays;
		std::vector<bool> shape_is_opaques;

		// position, normal, and texcoord(u, v) to be reorganized
		std::vector<vec4> position_and_us;
		std::vector<vec4> normal_and_vs;

		// index to be reorganized
		std::map<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>, uint32_t> index_from_tindex_combination;
		uint32_t index_counter = 0;

		for (size_t i_shape = 0; i_shape < shapes.size(); i_shape++)
		{
			// get shape
			const tinyobj::shape_t shape = shapes[i_shape];

			// construct shape indice
			std::vector<uint32_t> shape_indices;
			std::vector<uint32_t> shape_material_ids;

			// check indices
			bool is_shape_opaque = true;
			for (size_t i_index = 0; i_index < shape.mesh.indices.size(); i_index++)
			{
				const uint32_t vertex_index = uint32_t(shape.mesh.indices[i_index].vertex_index);
				const uint32_t normal_index = uint32_t(shape.mesh.indices[i_index].normal_index);
				const uint32_t texcoord_index = uint32_t(shape.mesh.indices[i_index].texcoord_index);
				const uint32_t material_id = uint32_t(shape.mesh.material_ids[i_index / 3]) + material_offset;
				const auto tindex_combination = std::make_tuple(vertex_index,
																normal_index,
																texcoord_index,
																material_id);
				const auto & find_result = index_from_tindex_combination.find(tindex_combination);

				if ((m_materials[material_id].m_flags & MATERIAL_FLAG_IS_OPAQUE) == 0)
				{
					is_shape_opaque = false;
				}

				// cannot find. this combination should be a new index
				uint32_t index = 0;
				if (find_result == index_from_tindex_combination.end())
				{
					// report index
					index = index_counter;

					// texcoord
					vec2 texcoord = texcoord_index == -1 ? vec2(0.0f, 0.0f) : vec2(attrib.texcoords[texcoord_index * 2 + 0],
																				   attrib.texcoords[texcoord_index * 2 + 1]);

					// push new position
					vec4 position_and_u(attrib.vertices[vertex_index * 3 + 0],
										attrib.vertices[vertex_index * 3 + 1],
										attrib.vertices[vertex_index * 3 + 2],
										texcoord.x);

					// dealing with normal
					vec3 normal;
					if (attrib.normals.size() == 0)
					{
						size_t face_indices[3];
						face_indices[0] = shape.mesh.indices[(i_index / 3) * 3 + 0].vertex_index;
						face_indices[1] = shape.mesh.indices[(i_index / 3) * 3 + 1].vertex_index;
						face_indices[2] = shape.mesh.indices[(i_index / 3) * 3 + 2].vertex_index;

						vec3 positions[3];
						for (size_t i = 0; i < 3; i++)
						{
							positions[i] = vec3(attrib.vertices[face_indices[i] * 3 + 0],
												attrib.vertices[face_indices[i] * 3 + 1],
												attrib.vertices[face_indices[i] * 3 + 2]);
						}

						// compute normal on our own
						normal = normalize(cross(positions[1] - positions[0], positions[2] - positions[0]));
					}
					else
					{
						normal = vec3(attrib.normals[normal_index * 3 + 0],
									  attrib.normals[normal_index * 3 + 1],
									  attrib.normals[normal_index * 3 + 2]);
					}
					vec4 normal_and_v(normal.x,
									  normal.y,
									  normal.z,
									  1.0f - texcoord.y);
					position_and_us.push_back(position_and_u);
					normal_and_vs.push_back(normal_and_v);

					// setup new index
					index_from_tindex_combination[tindex_combination] = index_counter++;
				}
				else
				{
					// report index
					index = find_result->second;
				}

				// push index
				shape_indices.push_back(index);
				if (i_index % 3 == 0)
				{
					shape_material_ids.push_back(material_id);
				}
			}

			// push into shapes indices
			shape_indices_arays.push_back(std::move(shape_indices));
			shape_material_ids_arrays.push_back(std::move(shape_material_ids));
			shape_is_opaques.push_back(is_shape_opaque);
		}

		/*
		* load all positions, normals and texcoords
		*/

		// create all basic buffers
		vk::BufferUsageFlags vertex_buffer_usage_flag = vk::BufferUsageFlagBits::eVertexBuffer;
		if (do_ray_tracing)
		{
			vertex_buffer_usage_flag |= vk::BufferUsageFlagBits::eStorageBuffer;
		}

		std::shared_ptr<Buffer> position_and_u_buffer = std::make_shared<Buffer>(position_and_us,
																				 vk::MemoryPropertyFlagBits::eDeviceLocal,
																				 vertex_buffer_usage_flag,
																				 vk::Format::eR32G32B32Sfloat);
		std::shared_ptr<Buffer> normal_and_v_buffer = std::make_shared<Buffer>(normal_and_vs,
																			   vk::MemoryPropertyFlagBits::eDeviceLocal,
																			   vertex_buffer_usage_flag,
																			   vk::Format::eR32G32B32Sfloat);

		/*
		* load all indices
		*/

		size_t begin = m_triangle_meshes.size();
		for (size_t i_shape = 0; i_shape < shapes.size(); i_shape++)
		{
			// create index buffer
			vk::BufferUsageFlags index_buffer_usage_flag = vk::BufferUsageFlagBits::eIndexBuffer;
			if (do_ray_tracing)
			{
				index_buffer_usage_flag |= vk::BufferUsageFlagBits::eStorageBuffer;
			}
			std::shared_ptr<Buffer> index_buffer = std::make_shared<Buffer>(shape_indices_arays[i_shape],
																			vk::MemoryPropertyFlagBits::eDeviceLocal,
																			index_buffer_usage_flag);

			std::shared_ptr<Buffer> material_id_buffer = nullptr;
			if (do_ray_tracing)
			{
				// create material buffer
				material_id_buffer = std::make_shared<Buffer>(shape_material_ids_arrays[i_shape],
															  vk::MemoryPropertyFlagBits::eDeviceLocal,
															  vk::BufferUsageFlagBits::eRayTracingNV | vk::BufferUsageFlagBits::eStorageBuffer);
			}

			// prepare triangle_mesh
			std::unique_ptr<TriangleMesh> triangle_mesh = std::make_unique<TriangleMesh>();
			{
				triangle_mesh->m_position_and_u_buffer = position_and_u_buffer;
				triangle_mesh->m_normal_and_v_buffer = normal_and_v_buffer;
				triangle_mesh->m_index_buffer = index_buffer;
				triangle_mesh->m_material_id_buffer = material_id_buffer;
				if (do_ray_tracing)
				{
					std::cout << std::to_string(shape_is_opaques[i_shape]) << std::endl;
					triangle_mesh->m_rt_blas = RtBlas(*position_and_u_buffer,
													  *index_buffer,
													  shape_is_opaques[i_shape]);
				}
			}
			m_triangle_meshes.push_back(std::move(triangle_mesh));
		}
		size_t end = m_triangle_meshes.size();

		return raw_ptrs(m_triangle_meshes, begin, end);
	}
};

struct TriangleMeshInstance
{
	TriangleMesh *	m_triangle_mesh;
	mat4			m_transform_model;
};
