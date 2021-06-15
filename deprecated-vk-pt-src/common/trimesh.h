#pragma once

// include tinyobj
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include "app_shader_shared/material.glsl.h"

#include "common/mortar.h"

#include "gavulkan/image.h"
#include "gavulkan/buffer.h"
#include "gavulkan/raytracingaccel.h"

struct ImageStorage
{
	std::vector<std::pair<RgbaImage<uint8_t>, bool>>							m_images; // image and is_opaque flag
	std::map<std::pair<std::filesystem::path, std::filesystem::path>, size_t>	m_image_index_from_path;

	std::vector<const RgbaImage<uint8_t> *>
	get_images()
	{
		std::vector<const RgbaImage<uint8_t> *> images;
		for (size_t i = 0; i < m_images.size(); i++)
		{
			images.push_back(&m_images[i].first);
		}
		return images;
	}

	// return index to image and whether it contains alpha channel or not
	std::pair<uint32_t, bool>
	fetch(const std::filesystem::path & path,
		  const bool do_check_alpha,
		  const std::filesystem::path & alpha_path = "")
	{
		// if could find image return the number
		if (m_image_index_from_path.find({ path, alpha_path }) != m_image_index_from_path.end())
		{
			const size_t image_index = m_image_index_from_path.at({ path, alpha_path });
			const bool is_opaque = m_images[image_index].second;
			return std::make_pair(static_cast<uint32_t>(image_index), is_opaque);
		}

		// check whether it has alpha channel or not
		StbiUint8Result raw_image(path);
		bool is_opaque = true;
		const int num_elements = raw_image.m_width * raw_image.m_height * 4;
		if (alpha_path != "")
		{
			StbiUint8Result alpha_image(alpha_path);
			THROW_ASSERT(alpha_image.m_width == raw_image.m_width && alpha_image.m_height == raw_image.m_height,
						 "main image and alpha image must share the same size");
			is_opaque = false;
			// copy alpha channel
			for (int i = 0; i < num_elements; i += 4)
			{
				const size_t r_offset = 0;
				const size_t a_offset = 3;
				raw_image.m_stbi_data[i + a_offset] = alpha_image.m_stbi_data[i + r_offset];
			}
		}
		else if (do_check_alpha)
		{
			for (int i = 3; i < num_elements; i += 4)
			{
				if (raw_image.m_stbi_data[i] != 255)
				{
					is_opaque = false;
					break;
				}
			}
		}

		// else load the image
		m_image_index_from_path[{ path, alpha_path }] = m_images.size();
		m_images.emplace_back(RgbaImage<uint8_t>(raw_image),
							  is_opaque);

		// and return the index
		return std::make_pair(static_cast<uint32_t>(m_images.size() - 1), is_opaque);
	}

	void
	generate_if_no_image()
	{
		// fill in the array with some random image just to stop vulkan from popping up the error messages
		if (m_images.size() > 0) return;

		std::vector<uint8_t> blank_image(4, 0);
		m_images.emplace_back(RgbaImage<uint8_t>(blank_image.data(),
												 uvec3(1u),
												 vk::ImageTiling::eOptimal,
												 RgbaImage<uint8_t>::RgbaUsageFlagBits),
							  false);
		m_images[0].first.init_sampler();
	}
};

struct TriangleMesh
{
	std::shared_ptr<Buffer>	m_position_and_u_buffer;
	std::shared_ptr<Buffer>	m_normal_and_v_buffer;
	std::shared_ptr<Buffer>	m_index_buffer;
	std::shared_ptr<Buffer>	m_material_id_buffer;
	std::shared_ptr<Buffer>	m_cdf_buffer;
	RtBlas					m_rt_blas;
	float					m_emission_weight = 0.0f;
	size_t					m_num_triangles;
};

struct TriangleMeshStorage
{
	std::vector<std::unique_ptr<TriangleMesh>>	m_triangle_meshes;
	std::vector<Material>						m_materials;
	std::shared_ptr<Buffer>						m_materials_buffer = nullptr;
	std::shared_ptr<ImageStorage>				m_image_storage = nullptr;

	// this buffer is used as a substitue when a geomtery is not an emittor
	std::shared_ptr<Buffer>						m_empty_pdf_cdf_buffer = nullptr;

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
													  vk::BufferUsageFlagBits::eStorageBuffer);
	}

	std::shared_ptr<Buffer>
	empty_pdf_cdf_buffer()
	{
		if (m_empty_pdf_cdf_buffer == nullptr)
		{
			std::vector<float> empty;
			empty.push_back(-1.0f);
			m_empty_pdf_cdf_buffer = std::make_shared<Buffer>(empty,
															  vk::MemoryPropertyFlagBits::eDeviceLocal,
															  vk::BufferUsageFlagBits::eStorageBuffer);
		}

		return m_empty_pdf_cdf_buffer;
	}

	// if an optional alpha_channel_texname is given,
	// this function will try to load channel R of the alpha_channel_texname
	// and insert the result into the alpha channel of main texture.
	template <typename T>
	std::pair<T, bool>
	get_encoded_color(const T & color,
					  const std::string & texname,
					  const std::filesystem::path & directory,
					  const bool do_check_alpha,
					  const std::string & alpha_channel_texname = "")
	{
		if (texname != "")
		{
			const auto tex_info = m_image_storage->fetch(directory / texname,
														 do_check_alpha,
														 alpha_channel_texname != "" ? (directory / alpha_channel_texname) : "");
			return std::make_pair(T(-static_cast<float>(tex_info.first) - 1.0f),
								  alpha_channel_texname == "" && tex_info.second);
		}
		else
		{
			THROW_ASSERT(alpha_channel_texname == "",
						 "does not support alpha channel texture when main texture is not available");
			return std::make_pair(color,
								  true);
		}
	}

	struct TinyObjLoadReturnType
	{
		tinyobj::attrib_t attrib;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> tmaterials;
	};
	TinyObjLoadReturnType
	tinyobj_load_materials(const std::filesystem::path & path,
						   vec3 * bbox_min,
						   vec3 * bbox_max)
	{
		// make sure that the path is absolute
		const std::filesystem::path abs_path = std::filesystem::absolute(path);

		TinyObjLoadReturnType result;

		// load mesh using tinyobj loader
		std::string warn;
		std::string err;
		tinyobj::LoadObj(&result.attrib,
						 &result.shapes,
						 &result.tmaterials,
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

		for (size_t i_vertex = 0; i_vertex < result.attrib.vertices.size() / 3; i_vertex++)
		{
			if (bbox_min)
			{
				bbox_min->x = std::min(result.attrib.vertices[i_vertex * 3 + 0], bbox_min->x);
				bbox_min->y = std::min(result.attrib.vertices[i_vertex * 3 + 1], bbox_min->y);
				bbox_min->z = std::min(result.attrib.vertices[i_vertex * 3 + 2], bbox_min->z);
			}

			if (bbox_max)
			{
				bbox_max->x = std::max(result.attrib.vertices[i_vertex * 3 + 0], bbox_max->x);
				bbox_max->y = std::max(result.attrib.vertices[i_vertex * 3 + 1], bbox_max->y);
				bbox_max->z = std::max(result.attrib.vertices[i_vertex * 3 + 2], bbox_max->z);
			}
		}
		return result;
	}

	// return material offset
	uint32_t
	load_materials(const std::vector<tinyobj::material_t> & tmaterials,
				   const std::filesystem::path & parent_directory)
	{
		const uint32_t material_offset = static_cast<uint32_t>(m_materials.size());

		for (size_t i = 0; i < tmaterials.size(); i++)
		{
			const auto & tmat = tmaterials[i];

			// convert float array to vec3
			const auto vec3_from_float3 = [](const float values[]) -> vec3
			{
				vec3 result;
				result.x = values[0];
				result.y = values[1];
				result.z = values[2];
				return result;
			};

			// if roughness is not set, we convert from exponent instead.
			const auto fetch_roughness = [](const tinyobj::material_t & tmat) -> float
			{
				if (tmat.roughness > 0.0f) { return tmat.roughness; }
				// the conversion is borrowed from Brian Karis, http://graphicrants.blogspot.com/2013/08/specular-brdf-reference.html
				// power = 2 / alpha2 - 2
				// I however found that using power = 2 / alpha - 2 gives better result
				const float alpha2 = 2.0f / (tmat.shininess + 2.0f);
				const float roughness = std::sqrt(alpha2);
				return roughness;
			};

			Material material = Material_create();
			const auto diffuse_refl = get_encoded_color(vec3_from_float3(tmat.diffuse),
														tmat.diffuse_texname,
														parent_directory,
														true,
														tmat.alpha_texname);
			const auto spec_refl = get_encoded_color(vec3_from_float3(tmat.specular),
													 tmat.specular_texname,
													 parent_directory,
													 false);
			const auto roughness = get_encoded_color(fetch_roughness(tmat),
													 tmat.roughness_texname,
													 parent_directory,
													 false);
			const auto emission = get_encoded_color(vec3_from_float3(tmat.emission),
													tmat.emissive_texname,
													parent_directory,
													false);
			material.m_diffuse_refl = diffuse_refl.first;
			material.m_spec_refl = spec_refl.first;
			material.m_roughness = roughness.first;
			material.m_emission = emission.first;
			if (tmat.illum == 7)
			{
				material.m_spec_trans = vec3_from_float3(tmat.transmittance);
				material.m_ior = tmat.ior;
			}
			const bool is_opaque = diffuse_refl.second;
			material.m_flags = is_opaque ? MATERIAL_FLAG_IS_OPAQUE : 0;
			m_materials.push_back(material);
		}

		return material_offset;
	}

	std::pair<std::vector<float>, float>
	compute_cdf_table_and_emissive_weight(const std::vector<uint32_t> indices,
										  const std::vector<vec4> position_and_us,
										  const std::vector<uint32_t> material_ids)
	{
		std::vector<float> cdf_table;
		std::vector<float> pdf_table;
		float emissive_weight = 0.0f;
		const size_t num_faces = indices.size() / 3;
		pdf_table = std::vector<float>(num_faces);
		cdf_table = std::vector<float>(num_faces + 1);

		for (size_t i_face = 0; i_face < num_faces; i_face++)
		{
			// find triangle area
			const vec3 & pos0 = vec3(position_and_us[indices[i_face * 3 + 0]]);
			const vec3 & pos1 = vec3(position_and_us[indices[i_face * 3 + 1]]);
			const vec3 & pos2 = vec3(position_and_us[indices[i_face * 3 + 2]]);
			const vec3 crossed = cross(pos1 - pos0, pos2 - pos0);
			const float triangle_area = dot(crossed, crossed) / 2.0f;

			// find emission value for that face or atleast approximation if texture is used
			const uint32_t mat_id = material_ids[i_face];
			const Material & mat = m_materials[mat_id];
			const vec3 emission = mat.m_emission.x > 0.0f ? mat.m_emission : vec3(1.0f); // for texture we just assume weight of 1

			// compute pdf weight
			const float pdf_weight = triangle_area * length(emission);
			emissive_weight += pdf_weight;

			pdf_table[i_face] = pdf_weight;
		}

		// normalize pdf_table
		for (size_t i_face = 0; i_face < num_faces; i_face++)
		{
			pdf_table[i_face] /= emissive_weight;
		}

		// compute cdf_table
		cdf_table[0] = 0.0f;
		for (size_t i_face = 0; i_face < num_faces; i_face++)
		{
			cdf_table[i_face + 1] = pdf_table[i_face] + cdf_table[i_face];
		}
		THROW_ASSERT(std::abs(cdf_table[num_faces] - 1.0f) <= 1e-2f,
					 "last element of cdf table is not 1.0f");
		cdf_table[num_faces] = 1.0f;

		return std::make_pair(cdf_table,
							  emissive_weight);
	}

	// add mesh via this method will seperate mesh into 3 parts, emissive, non-opaque and opaque
	std::vector<TriangleMesh *>
	add_obj_mesh_single(const std::filesystem::path & path,
						const bool do_ray_tracing,
						vec3 * bbox_min,
						vec3 * bbox_max)
	{
		/*
		* load all shapes
		*/

		auto loaded_tshapes = tinyobj_load_materials(path,
													 bbox_min,
													 bbox_max);
		const uint32_t material_offset = load_materials(loaded_tshapes.tmaterials,
														path.parent_path());
		const std::vector<tinyobj::shape_t> & shapes = loaded_tshapes.shapes;
		const tinyobj::attrib_t & attrib = loaded_tshapes.attrib;

		/*
		* reorganize the data to make sure that vertex index = texcoord index = normal index
		*/

		// indices for all shape
		std::vector<uint32_t> emissive_nonopaque_indices;
		std::vector<uint32_t> emissive_nonopaque_material_ids;
		std::vector<uint32_t> nonemissive_nonopaque_indices;
		std::vector<uint32_t> nonemissive_nonopaque_material_ids;
		std::vector<uint32_t> emissive_opaque_indices;
		std::vector<uint32_t> emissive_opaque_material_ids;
		std::vector<uint32_t> nonemissive_opaque_indices;
		std::vector<uint32_t> nonemissive_opaque_material_ids;
		std::vector<float> shape_emissive_weights;

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

			for (size_t i_index = 0; i_index < shape.mesh.indices.size(); i_index++)
			{
				const uint32_t vertex_index = static_cast<uint32_t>(shape.mesh.indices[i_index].vertex_index);
				const uint32_t normal_index = static_cast<uint32_t>(shape.mesh.indices[i_index].normal_index);
				const uint32_t texcoord_index = static_cast<uint32_t>(shape.mesh.indices[i_index].texcoord_index);
				const uint32_t material_id = static_cast<uint32_t>(shape.mesh.material_ids[i_index / 3]) + material_offset;
				const auto tindex_combination = std::make_tuple(vertex_index,
																normal_index,
																texcoord_index,
																material_id);
				const auto & find_result = index_from_tindex_combination.find(tindex_combination);

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

				bool is_emissive = false;
				if (length(m_materials[material_id].m_emission) > 0.0f)
				{
					is_emissive = true;
				}

				bool is_opaque = true;
				if ((m_materials[material_id].m_flags & MATERIAL_FLAG_IS_OPAQUE) == 0)
				{
					is_opaque = false;
				}

				// add index
				if (is_emissive && is_opaque)
				{
					emissive_opaque_indices.push_back(index);
				}
				else if (is_emissive && !is_opaque)
				{
					emissive_nonopaque_indices.push_back(index);
				}
				else if (!is_emissive && is_opaque)
				{
					nonemissive_opaque_indices.push_back(index);
				}
				else // !is_emissive && !is_opaque
				{
					nonemissive_nonopaque_indices.push_back(index);
				}

				// add material_id
				if (i_index % 3 == 0)
				{
					if (is_emissive && is_opaque)
					{
						emissive_opaque_material_ids.push_back(material_id);
					}
					else if (is_emissive && !is_opaque)
					{
						emissive_nonopaque_material_ids.push_back(material_id);
					}
					else if (!is_emissive && is_opaque)
					{
						nonemissive_opaque_material_ids.push_back(material_id);
					}
					else // !is_emissive && !is_opaque
					{
						nonemissive_nonopaque_material_ids.push_back(material_id);
					}
				}
			}
		} // end for all shapes


		/*
		* load all positions, normals and texcoords
		*/

		// create all basic buffers
		std::shared_ptr<Buffer> position_and_u_buffer = std::make_shared<Buffer>(position_and_us,
																				 vk::MemoryPropertyFlagBits::eDeviceLocal,
																				 vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress);
		std::shared_ptr<Buffer> normal_and_v_buffer = std::make_shared<Buffer>(normal_and_vs,
																			   vk::MemoryPropertyFlagBits::eDeviceLocal,
																			   vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer);

		auto create_triangle_mesh = [&](const std::shared_ptr<Buffer> & position_and_u_buffer,
										const std::shared_ptr<Buffer> & normal_and_v_buffer,
										const std::vector<uint32_t> & indices,
										const std::vector<uint32_t> & material_ids,
										const bool is_shape_opaque,
										const std::vector<float> cdf_table,
										const float emissive_weight)
		{
			if (indices.size() == 0) return;

			std::unique_ptr<TriangleMesh> triangle_mesh = std::make_unique<TriangleMesh>();
			triangle_mesh->m_position_and_u_buffer = position_and_u_buffer;
			triangle_mesh->m_normal_and_v_buffer = normal_and_v_buffer;
			triangle_mesh->m_index_buffer = std::make_shared<Buffer>(indices,
																	 vk::MemoryPropertyFlagBits::eDeviceLocal,
																	 vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress);
			triangle_mesh->m_material_id_buffer = std::make_shared<Buffer>(material_ids,
																		   vk::MemoryPropertyFlagBits::eDeviceLocal,
																		   vk::BufferUsageFlagBits::eStorageBuffer);
			triangle_mesh->m_num_triangles = indices.size() / 3;

			if (do_ray_tracing)
			{
				triangle_mesh->m_rt_blas = RtBlas(*position_and_u_buffer,
												  *triangle_mesh->m_index_buffer,
												  is_shape_opaque);
				triangle_mesh->m_emission_weight = emissive_weight;
				if (cdf_table.size() == 0)
				{
					triangle_mesh->m_cdf_buffer = empty_pdf_cdf_buffer();
				}
				else
				{
					triangle_mesh->m_cdf_buffer = std::make_shared<Buffer>(cdf_table,
																		   vk::MemoryPropertyFlagBits::eDeviceLocal,
																		   vk::BufferUsageFlagBits::eStorageBuffer);;
				}
			}

			m_triangle_meshes.push_back(std::move(triangle_mesh));
		};

		std::vector<float> emissive_opaque_cdf_table = std::vector<float>();
		std::vector<float> nonemissive_opaque_cdf_table = std::vector<float>();
		std::vector<float> emissive_nonopaque_cdf_table = std::vector<float>();
		std::vector<float> nonemissive_nonopaque_cdf_table = std::vector<float>();
		float emissive_opaque_weight = 0.0f;
		float nonemissive_opaque_weight = 0.0f;
		float emissive_nonopaque_weight = 0.0f;
		float nonemissive_nonopaque_weight = 0.0f;

		if (emissive_opaque_indices.size() > 0)
		{
			auto eo = compute_cdf_table_and_emissive_weight(emissive_opaque_indices,
															position_and_us,
															emissive_opaque_material_ids);
			emissive_opaque_cdf_table = eo.first;
			emissive_opaque_weight = eo.second;
		}

		if (emissive_nonopaque_indices.size() > 0)
		{
			auto en = compute_cdf_table_and_emissive_weight(emissive_nonopaque_indices,
															position_and_us,
															emissive_nonopaque_material_ids);
			emissive_nonopaque_cdf_table = en.first;
			emissive_nonopaque_weight = en.second;
		}

		const std::array<std::vector<uint32_t>, 4>
			indices_arrays = {		emissive_nonopaque_indices,		emissive_opaque_indices,		nonemissive_nonopaque_indices,		nonemissive_opaque_indices };
		const std::array<std::vector<uint32_t>, 4>
			material_ids_arrays = { emissive_nonopaque_material_ids,emissive_opaque_material_ids,	nonemissive_nonopaque_material_ids, nonemissive_opaque_material_ids };
		const std::array<std::vector<float>, 4>
			emission_cdf_tables = { emissive_nonopaque_cdf_table,	emissive_opaque_cdf_table,		nonemissive_nonopaque_cdf_table,	nonemissive_opaque_cdf_table };
		const std::array<float, 4>
			emissive_weights = {	emissive_nonopaque_weight,		emissive_opaque_weight,			nonemissive_nonopaque_weight,		nonemissive_opaque_weight };
		const std::array<bool, 4>
			is_opaques =	{ 		false,							true,							false,								true};


		const size_t begin = m_triangle_meshes.size();
		for (size_t i = 0; i < 4; i++)
		{
			create_triangle_mesh(position_and_u_buffer,
								 normal_and_v_buffer,
								 indices_arrays[i],
								 material_ids_arrays[i],
								 is_opaques[i],
								 emission_cdf_tables[i],
								 emissive_weights[i]);
		}
		const size_t end = m_triangle_meshes.size();

		return raw_ptrs(m_triangle_meshes, begin, end);
	}

	std::vector<TriangleMesh *>
	add_obj_mesh(const std::filesystem::path & path,
				 const bool do_ray_tracing,
				 vec3 * bbox_min,
				 vec3 * bbox_max)
	{
		/*
		* load all shapes
		*/

		auto loaded_tshapes = tinyobj_load_materials(path,
													 bbox_min,
													 bbox_max);
		const uint32_t material_offset = load_materials(loaded_tshapes.tmaterials,
														path.parent_path());
		const std::vector<tinyobj::shape_t> & shapes = loaded_tshapes.shapes;
		const tinyobj::attrib_t & attrib = loaded_tshapes.attrib;

		/*
		* reorganize the data to make sure that vertex index = texcoord index = normal index
		*/

		// indices for all shape
		std::vector<std::vector<uint32_t>> shape_indices_arays;
		std::vector<std::vector<uint32_t>> shape_material_ids_arrays;
		std::vector<std::vector<float>> shape_cdf_table_arrays;
		std::vector<float> shape_emissive_weights;
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
			bool is_shape_emissive = false;
			for (size_t i_index = 0; i_index < shape.mesh.indices.size(); i_index++)
			{
				const uint32_t vertex_index = static_cast<uint32_t>(shape.mesh.indices[i_index].vertex_index);
				const uint32_t normal_index = static_cast<uint32_t>(shape.mesh.indices[i_index].normal_index);
				const uint32_t texcoord_index = static_cast<uint32_t>(shape.mesh.indices[i_index].texcoord_index);
				const uint32_t material_id = static_cast<uint32_t>(shape.mesh.material_ids[i_index / 3]) + material_offset;
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

					// check whether material has emission or not
					const Material & mat = m_materials[material_id];
					if (length(mat.m_emission) > SMALL_VALUE)
					{
						is_shape_emissive = true;
					}
				}
			}

			// pdf and cdf table
			auto [cdf_table, emissive_weight] = compute_cdf_table_and_emissive_weight(shape_indices,
																					  position_and_us,
																					  shape_material_ids);

			// push into shapes indices
			shape_indices_arays.push_back(std::move(shape_indices));
			shape_material_ids_arrays.push_back(std::move(shape_material_ids));
			shape_is_opaques.push_back(is_shape_opaque);
			shape_cdf_table_arrays.push_back(std::move(cdf_table));
			shape_emissive_weights.push_back(emissive_weight);
		}

		/*
		* load all positions, normals and texcoords
		*/

		// create all basic buffers
		std::shared_ptr<Buffer> position_and_u_buffer = std::make_shared<Buffer>(position_and_us,
																				 vk::MemoryPropertyFlagBits::eDeviceLocal,
																				 vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer);
		std::shared_ptr<Buffer> normal_and_v_buffer = std::make_shared<Buffer>(normal_and_vs,
																			   vk::MemoryPropertyFlagBits::eDeviceLocal,
																			   vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer);

		/*
		* load all indices
		*/

		size_t begin = m_triangle_meshes.size();
		for (size_t i_shape = 0; i_shape < shapes.size(); i_shape++)
		{
			// create index buffer
			std::shared_ptr<Buffer> index_buffer = std::make_shared<Buffer>(shape_indices_arays[i_shape],
																			vk::MemoryPropertyFlagBits::eDeviceLocal,
																			vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eStorageBuffer);

			// create material id buffer
			// and create pdf and cdf table
			std::shared_ptr<Buffer> material_id_buffer = nullptr;
			std::shared_ptr<Buffer> cdf_table_buffer = nullptr;
			if (do_ray_tracing)
			{
				material_id_buffer = std::make_shared<Buffer>(shape_material_ids_arrays[i_shape],
															  vk::MemoryPropertyFlagBits::eDeviceLocal,
															  vk::BufferUsageFlagBits::eStorageBuffer);

				if (shape_emissive_weights[i_shape] > SMALL_VALUE)
				{
					cdf_table_buffer = std::make_shared<Buffer>(shape_cdf_table_arrays[i_shape],
														 vk::MemoryPropertyFlagBits::eDeviceLocal,
														 vk::BufferUsageFlagBits::eStorageBuffer);
				}
				else
				{
					cdf_table_buffer = empty_pdf_cdf_buffer();
				}
			}

			// prepare triangle_mesh
			std::unique_ptr<TriangleMesh> triangle_mesh = std::make_unique<TriangleMesh>();
			{
				triangle_mesh->m_position_and_u_buffer = position_and_u_buffer;
				triangle_mesh->m_normal_and_v_buffer = normal_and_v_buffer;
				triangle_mesh->m_index_buffer = index_buffer;
				triangle_mesh->m_material_id_buffer = material_id_buffer;
				triangle_mesh->m_num_triangles = shape_indices_arays[i_shape].size() / 3;
				if (do_ray_tracing)
				{
					triangle_mesh->m_rt_blas = RtBlas(*position_and_u_buffer,
													  *index_buffer,
													  shape_is_opaques[i_shape]);
					triangle_mesh->m_emission_weight = shape_emissive_weights[i_shape];
					triangle_mesh->m_cdf_buffer = cdf_table_buffer;
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
