#pragma once

#include "common/camera.h"
#include "common/ste/stevector.h"
#include "loader/img_loader.h"
#include "render/common/compact_vertex.h"
#include "render/common/engine_setting.h"
#include "render/common/standard_material_ref.h"
#include "rhi/rhi.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <filesystem>
#include <scene_graph.h>

struct StandardMesh
{
    size_t m_vertex_buffer_id;
    size_t m_index_buffer_id;
    size_t m_vertex_buffer_offset;
    size_t m_index_buffer_offset;
    size_t m_num_vertices;
    size_t m_num_indices;
};

struct SceneGeometry
{
    uint32_t m_vbuf_offset = 0;
    uint32_t m_ibuf_offset = 0;
    size_t m_num_vertices  = 0;
    size_t m_num_indices   = 0;
    uint32_t m_material_id = 0;
    bool m_is_updatable    = false;
};

struct SceneBaseInstance
{
    std::vector<urange> m_geometry_id_ranges = {};
};

struct SceneInstance
{
    uint32_t m_base_instance_id = 0;
    float4x4 m_transform        = glm::identity<float4x4>();
};

struct SceneDesc
{
    std::vector<SceneInstance> m_instances = {};
};

struct SceneResource
{
    enum class LoadStandardParamEnum
    {
        DiffuseReflectance,
        SpecularReflectance,
        SpecularRoughness
    };

    const Rhi::Device * m_device = nullptr;

    // command pool
    Rhi::CommandPool m_transfer_cmd_pool;

    // device position, packed and index information
    Rhi::IndexType m_ibuf_index_type     = Rhi::IndexType::Uint16;
    Rhi::FormatEnum m_vbuf_position_type = Rhi::FormatEnum::R32G32B32_SFloat;
    Rhi::Buffer m_d_vbuf_position        = {};
    Rhi::Buffer m_d_vbuf_packed          = {};
    Rhi::Buffer m_d_ibuf                 = {};
    size_t m_vbuf_num_vertices           = 0;
    size_t m_ibuf_num_indices            = 0;

    // device & host textures and materials
    Std::FsVector<Rhi::Texture, EngineSetting::MaxNumBindlessTextures> m_d_textures;
    Rhi::Buffer m_d_materials;
    Std::FsVector<StandardMaterial, EngineSetting::MaxNumStandardMaterials> m_h_materials;

    // device & host lookup table for geometry & instance
    // look up offset into geometry table based on instance index
    Rhi::Buffer m_d_rt_instance_table = {};
    Rhi::Buffer m_d_rt_geometry_table = {};

    // device & host blas (requires update)
    std::vector<Rhi::RayTracingBlas> m_rt_blases;
    Rhi::RayTracingTlas m_rt_tlas;

    // camera
    FpsCamera m_camera;

    // scene graph
    SceneGraphNode m_scene_graph_root = SceneGraphNode(false);
    Std::FsVector<SceneGeometry, EngineSetting::MaxNumGeometries> m_geometries;
    Std::FsVector<SceneBaseInstance, EngineSetting::MaxNumInstances> m_base_instances;

    // TODO:: too complicated for now
    // std::vector<SceneGraphNode> m_scene_static_instances;
    // std::vector<SceneGraphNode> m_scene_dynamic_instances;

    SceneResource() {}

    SceneResource(const Rhi::Device & device) : m_device(&device)
    {
        m_transfer_cmd_pool = Rhi::CommandPool(&device, Rhi::CommandQueueType::Transfer);

        // index buffer vertex buffer
        m_d_vbuf_position = Rhi::Buffer(m_device,
                                        Rhi::BufferUsageEnum::TransferDst,
                                        Rhi::MemoryUsageEnum::GpuOnly,
                                        sizeof(float3) * EngineSetting::MaxNumVertices,
                                        "scene_m_d_vbuf_position");
        m_d_vbuf_packed   = Rhi::Buffer(m_device,
                                      Rhi::BufferUsageEnum::TransferDst,
                                      Rhi::MemoryUsageEnum::GpuOnly,
                                      sizeof(CompactVertex) * EngineSetting::MaxNumVertices,
                                      "scene_m_d_vbuf_packed");
        m_d_ibuf          = Rhi::Buffer(m_device,
                               Rhi::BufferUsageEnum::TransferDst,
                               Rhi::MemoryUsageEnum::GpuOnly,
                               Rhi::GetSizeInBytes(m_ibuf_index_type) * EngineSetting::MaxNumIndices,
                               "scene_m_d_ibuf");

        // materials
        m_d_materials = Rhi::Buffer(m_device,
                                    Rhi::BufferUsageEnum::ConstantBuffer,
                                    Rhi::MemoryUsageEnum::CpuOnly,
                                    sizeof(StandardMaterial) * EngineSetting::MaxNumStandardMaterials,
                                    "scene_m_d_materials");

        // geometry table and geometry offset table
        m_d_rt_instance_table = Rhi::Buffer(m_device,
                                            Rhi::BufferUsageEnum::TransferDst,
                                            Rhi::MemoryUsageEnum::CpuOnly,
                                            sizeof(uint32_t) * EngineSetting::MaxNumGeometryOffsetTableEntry,
                                            "scene_m_d_geometry_table_offset");
        m_d_rt_geometry_table = Rhi::Buffer(m_device,
                                            Rhi::BufferUsageEnum::TransferDst,
                                            Rhi::MemoryUsageEnum::CpuOnly,
                                            sizeof(uint32_t) * EngineSetting::MaxNumGeometryTableEntry,
                                            "scene_m_d_geometry_table");
    }

    urange
    add_geometries(const std::filesystem::path & path, Rhi::StagingBufferManager & staging_buffer_manager)
    {
        Assimp::Importer importer;
        const aiScene * scene =
            importer.ReadFile(path.string(),
                              aiProcess_Triangulate | aiProcess_CalcTangentSpace | aiProcess_GenSmoothNormals |
                                  aiProcess_JoinIdenticalVertices | aiProcess_RemoveRedundantMaterials);

        auto to_float3 = [&](const aiVector3D & vec3) { return float3(vec3.x, vec3.y, vec3.z); };
        auto to_float2 = [&](const aiVector2D & vec2) { return float2(vec2.x, vec2.y); };

        const unsigned int num_meshes = scene->mNumMeshes;
        const urange result(m_geometries.length(), m_geometries.length() + num_meshes);

        // load all materials (and necessary textures)
        const unsigned int material_offset = m_h_materials.length();
        for (size_t i_mat = 0; i_mat < scene->mNumMaterials; i_mat++)
        {
            StandardMaterial mat =
                add_standard_material(path, *scene->mMaterials[i_mat], staging_buffer_manager);
            m_h_materials.push_back(mat);
        }

        // upload vertices and indices
        {
            unsigned int total_num_vertices = 0;
            unsigned int total_num_indices  = 0;
            for (unsigned int i_mesh = 0; i_mesh < num_meshes; i_mesh++)
            {
                const aiMesh * aimesh = scene->mMeshes[i_mesh];
                total_num_vertices += aimesh->mNumVertices;
                total_num_indices += aimesh->mNumFaces * 3;

                // round so the index is align by 32
                total_num_indices  = round_up(total_num_indices, 32);
                total_num_vertices = round_up(total_num_vertices, 32);
            }

            std::vector<float3> vb_positions(total_num_vertices);
            std::vector<CompactVertex> vb_packed_info(total_num_vertices);
            std::vector<uint16_t> ib(total_num_indices);

            unsigned int vertex_buffer_offset = 0;
            unsigned int index_buffer_offset  = 0;
            for (unsigned int i_mesh = 0; i_mesh < num_meshes; i_mesh++)
            {
                // all information
                const aiMesh * aimesh       = scene->mMeshes[i_mesh];
                const uint32_t num_vertices = static_cast<uint32_t>(aimesh->mNumVertices);
                const uint32_t num_faces    = static_cast<uint32_t>(aimesh->mNumFaces);
                if (num_faces * 3 > std::numeric_limits<uint16_t>::max())
                {
                    Logger::Critical<true>(__FUNCTION__,
                                           " the framework does not support num faces per mesh > "
                                           "MAX_UINT16");
                }

                for (unsigned int i_vertex = 0; i_vertex < num_vertices; i_vertex++)
                {
                    // setup positions
                    float3 & pos = vb_positions[vertex_buffer_offset + i_vertex];
                    pos          = to_float3(aimesh->mVertices[i_vertex]);

                    // setup compact information
                    CompactVertex & vertex = vb_packed_info[vertex_buffer_offset + i_vertex];
                    vertex.m_position      = to_float3(aimesh->mVertices[i_vertex]);
                    vertex.m_snormal       = to_float3(aimesh->mNormals[i_vertex]);
                    vertex.m_texcoord_x    = aimesh->mTextureCoords[0][i_vertex].x;
                    vertex.m_texcoord_y    = aimesh->mTextureCoords[0][i_vertex].y;
                }

                // set faces
                for (unsigned int i_face = 0; i_face < num_faces; i_face++)
                {
                    for (size_t i_vertex = 0; i_vertex < 3; i_vertex++)
                    {
                        uint16_t & index = ib[index_buffer_offset + i_face * 3 + i_vertex];
                        index = static_cast<uint16_t>(aimesh->mFaces[i_face].mIndices[i_vertex]);
                    }
                }

                vertex_buffer_offset += aimesh->mNumVertices;
                index_buffer_offset += aimesh->mNumFaces * 3;
                vertex_buffer_offset = round_up(vertex_buffer_offset, 32);
                index_buffer_offset  = round_up(index_buffer_offset, 32);
            }

            Rhi::CommandList cmd_list = m_transfer_cmd_pool.get_command_list();

            Rhi::Fence tmp_fence(staging_buffer_manager.m_device);
            tmp_fence.reset();
            Rhi::Buffer staging_buffer(m_device,
                                       Rhi::BufferUsageEnum::TransferSrc,
                                       Rhi::MemoryUsageEnum::CpuOnly,
                                       vb_positions.size() * sizeof(vb_positions[0]));
            Rhi::Buffer staging_buffer2(m_device,
                                        Rhi::BufferUsageEnum::TransferSrc,
                                        Rhi::MemoryUsageEnum::CpuOnly,
                                        ib.size() * sizeof(ib[0]));
            std::memcpy(staging_buffer.map(), vb_positions.data(), vb_positions.size() * sizeof(vb_positions[0]));
            std::memcpy(staging_buffer2.map(), ib.data(), ib.size() * sizeof(ib[0]));
            staging_buffer.unmap();
            staging_buffer2.unmap();

            cmd_list.begin();
            cmd_list.copy_buffer_region(m_d_vbuf_position,
                                        m_vbuf_num_vertices * sizeof(float3),
                                        staging_buffer,
                                        0,
                                        vb_positions.size() * sizeof(vb_positions[0]));
            cmd_list.copy_buffer_region(m_d_ibuf,
                                        m_ibuf_num_indices * Rhi::GetSizeInBytes(m_ibuf_index_type),
                                        staging_buffer2,
                                        0,
                                        ib.size() * sizeof(ib[0]));
            cmd_list.end();
            cmd_list.submit(&tmp_fence);
            tmp_fence.wait();

            vertex_buffer_offset = m_vbuf_num_vertices;
            index_buffer_offset  = m_ibuf_num_indices;
            for (unsigned int i_mesh = 0; i_mesh < num_meshes; i_mesh++)
            {
                // all information
                const aiMesh * aimesh       = scene->mMeshes[i_mesh];
                const uint32_t num_vertices = static_cast<uint32_t>(aimesh->mNumVertices);
                const uint32_t num_faces    = static_cast<uint32_t>(aimesh->mNumFaces);

                SceneGeometry model;
                model.m_vbuf_offset  = vertex_buffer_offset;
                model.m_ibuf_offset  = index_buffer_offset;
                model.m_num_indices  = num_faces * 3;
                model.m_num_vertices = num_vertices;
                model.m_is_updatable = true;
                model.m_material_id  = material_offset + aimesh->mMaterialIndex;
                m_geometries.push_back(model);

                vertex_buffer_offset += aimesh->mNumVertices;
                index_buffer_offset += aimesh->mNumFaces * 3;
                vertex_buffer_offset = round_up(vertex_buffer_offset, 32);
                index_buffer_offset  = round_up(index_buffer_offset, 32);
            }

            m_vbuf_num_vertices += total_num_vertices;
            m_ibuf_num_indices += total_num_indices;
        }

        return result;
    }

    size_t
    add_base_instance(const std::span<urange> & geometry_ranges)
    {
        SceneBaseInstance base_instance;
        base_instance.m_geometry_id_ranges =
            std::vector<urange>(geometry_ranges.begin(), geometry_ranges.end());
        m_base_instances.push_back(base_instance);
        return m_base_instances.length() - 1;
    }

    StandardMaterial
    add_standard_material(const std::filesystem::path & path,
                          const aiMaterial & material,
                          Rhi::StagingBufferManager & staging_buffer_manager)
    {
        StandardMaterial standard_material;
        standard_material.m_diffuse_tex_id =
            add_texture(path, material, LoadStandardParamEnum::DiffuseReflectance, staging_buffer_manager);
        standard_material.m_specular_tex_id =
            add_texture(path, material, LoadStandardParamEnum::SpecularReflectance, staging_buffer_manager);
        standard_material.m_roughness_tex_id =
            add_texture(path, material, LoadStandardParamEnum::SpecularRoughness, staging_buffer_manager);
        return standard_material;
    }

    template <size_t NumChannels>
    size_t
    add_texture(const std::array<uint8_t, NumChannels> & v, Rhi::StagingBufferManager & staging_buffer_manager)
    {
        // load using stbi
        static_assert(NumChannels == 4 || NumChannels == 1, "num channels must be 4 or 1");
        Rhi::FormatEnum format_enum = v.size() == 4 ? Rhi::FormatEnum::R8G8B8A8_UNorm : Rhi::FormatEnum::R8_UNorm;
        Rhi::Texture texture(staging_buffer_manager.m_device,
                             Rhi::TextureUsageEnum::Sampled,
                             Rhi::TextureStateEnum::FragmentShaderVisible,
                             format_enum,
                             int2(1, 1),
                             reinterpret_cast<const std::byte *>(v.data()),
                             &staging_buffer_manager,
                             float4(),
                             "");
        staging_buffer_manager.submit_all_pending_upload();

        // emplace back
        m_d_textures.emplace_back(std::move(texture));
        return m_d_textures.length() - 1;
    }

    size_t
    add_texture(const std::filesystem::path & path, const size_t desired_channel, Rhi::StagingBufferManager & staging_buffer_manager)
    {
        const std::string filepath_str = path.string();

        // load using stbi
        int2 resolution;
        stbi_set_flip_vertically_on_load(true);
        void * image =
            stbi_load(filepath_str.c_str(), &resolution.x, &resolution.y, nullptr, static_cast<int>(desired_channel));
        assert(image);
        std::byte * image_bytes = reinterpret_cast<std::byte *>(image);
        assert(desired_channel == 4 || desired_channel == 1);
        Rhi::FormatEnum format_enum =
            desired_channel == 4 ? Rhi::FormatEnum::R8G8B8A8_UNorm : Rhi::FormatEnum::R8_UNorm;
        Rhi::Texture texture(staging_buffer_manager.m_device,
                             Rhi::TextureUsageEnum::Sampled,
                             Rhi::TextureStateEnum::FragmentShaderVisible,
                             format_enum,
                             resolution,
                             image_bytes,
                             &staging_buffer_manager,
                             float4(),
                             filepath_str);
        staging_buffer_manager.submit_all_pending_upload();
        stbi_image_free(image);

        // emplace back
        m_d_textures.emplace_back(std::move(texture));
        return m_d_textures.length() - 1;
    }

    size_t
    add_texture(const std::filesystem::path & path,
                const aiMaterial & material,
                const LoadStandardParamEnum standard_param,
                Rhi::StagingBufferManager & staging_buffer_manager)
    {
        aiTextureType ai_tex_type = aiTextureType::aiTextureType_NONE;
        const char * ai_mat_key   = nullptr;
        int num_desired_channel   = 0;
        if (standard_param == LoadStandardParamEnum::DiffuseReflectance)
        {
            ai_tex_type         = aiTextureType::aiTextureType_DIFFUSE;
            ai_mat_key          = AI_MATKEY_COLOR_DIFFUSE;
            num_desired_channel = 4;
        }
        else if (standard_param == LoadStandardParamEnum::SpecularReflectance)
        {
            ai_tex_type         = aiTextureType::aiTextureType_SPECULAR;
            ai_mat_key          = AI_MATKEY_COLOR_SPECULAR;
            num_desired_channel = 4;
        }
        else if (standard_param == LoadStandardParamEnum::SpecularRoughness)
        {
            ai_tex_type         = aiTextureType::aiTextureType_SHININESS;
            ai_mat_key          = AI_MATKEY_COLOR_DIFFUSE;
            num_desired_channel = 1;
        }

        aiString tex_name;
        aiColor4D color;
        if (material.GetTexture(ai_tex_type, 0, &tex_name) == aiReturn_SUCCESS)
        {
            const std::filesystem::path tex_path = path.parent_path() / std::string(tex_name.C_Str());
            return add_texture(tex_path, num_desired_channel, staging_buffer_manager);
        }
        else if (aiGetMaterialColor(&material, ai_mat_key, 0, 0, &color) == aiReturn_SUCCESS)
        {
            if (num_desired_channel == 1)
            {
                const uint8_t r = static_cast<uint8_t>(color.r * 256);
                return add_texture(std::array<uint8_t, 1>{ r }, staging_buffer_manager);
            }
            else if (num_desired_channel == 4)
            {
                const uint8_t r = static_cast<uint8_t>(color.r * 256);
                const uint8_t g = static_cast<uint8_t>(color.g * 256);
                const uint8_t b = static_cast<uint8_t>(color.b * 256);
                const uint8_t a = static_cast<uint8_t>(color.a * 256);
                return add_texture(std::array<uint8_t, 4>{ r, g, b, a }, staging_buffer_manager);
            }
        }

        // should not reach this point
        assert(false);

        return static_cast<size_t>(0);
    };

    void
    commit(const SceneDesc & scene_desc, Rhi::StagingBufferManager & staging_buffer_manager)
    {
        // create blas for all base instance
        {
            std::vector<Rhi::RayTracingGeometryDesc> geom_descs;
            m_rt_blases.resize(m_base_instances.length());

            for (size_t i_binst = 0; i_binst < m_base_instances.length(); i_binst++)
            {
                SceneBaseInstance & base_instance = m_base_instances[i_binst];

                // prepare descs
                geom_descs.clear();

                // populate geometry descs
                bool is_updatable = false;
                for (size_t i_geom = 0; i_geom < base_instance.m_geometry_id_ranges.size(); i_geom++)
                {
                    const urange & gid_range = base_instance.m_geometry_id_ranges[i_geom];
                    for (uint32_t geometry_id = gid_range.m_begin; geometry_id < gid_range.m_end; geometry_id++)
                    {
                        const SceneGeometry & geometry = m_geometries[geometry_id];
                        if (geometry.m_is_updatable)
                        {
                            is_updatable = true;
                        }
                        Rhi::RayTracingGeometryDesc geom_desc;
                        geom_desc.set_flag(Rhi::RayTracingGeometryFlag::Opaque);
                        geom_desc.set_index_buffer(m_d_ibuf,
                                                   geometry.m_ibuf_offset * Rhi::GetSizeInBytes(m_ibuf_index_type),
                                                   m_ibuf_index_type,
                                                   geometry.m_num_indices);
                        geom_desc.set_vertex_buffer(m_d_vbuf_position,
                                                    geometry.m_vbuf_offset * Rhi::GetSizeInBytes(m_vbuf_position_type),
                                                    m_vbuf_position_type,
                                                    Rhi::GetSizeInBytes(m_vbuf_position_type),
                                                    geometry.m_num_vertices);
                        geom_descs.push_back(geom_desc);
                    }
                }

                // decides hint
                Rhi::RayTracingBuildHint hint = is_updatable ? Rhi::RayTracingBuildHint::Deformable
                                                             : Rhi::RayTracingBuildHint::NonDeformable;

                // build blas
                m_rt_blases[i_binst] = Rhi::RayTracingBlas(m_device, geom_descs, hint, &staging_buffer_manager);
                staging_buffer_manager.submit_all_pending_upload();
            }
        }

        // TODO:: move tlas to async compute
        // build tlas
        {
            // populate instances list to be build tlas
            std::vector<Rhi::RayTracingInstance> instances(scene_desc.m_instances.size());
            for (size_t i_inst = 0; i_inst < scene_desc.m_instances.size(); i_inst++)
            {
                const size_t base_instance_id = scene_desc.m_instances[i_inst].m_base_instance_id;
                const Rhi::RayTracingBlas & blas = m_rt_blases[base_instance_id];
                instances[i_inst] =
                    Rhi::RayTracingInstance(blas, scene_desc.m_instances[i_inst].m_transform, 0, base_instance_id);
            }

            // build tlas
            m_rt_tlas = Rhi::RayTracingTlas(m_device,
                                            instances,
                                            &staging_buffer_manager,
                                            "ray_tracing_tlas");
            staging_buffer_manager.submit_all_pending_upload();
        }

        // build material buffer
        {
            StandardMaterial * smat = static_cast<StandardMaterial *>(m_d_materials.map());
            for (size_t i = 0; i < m_h_materials.max_size(); i++)
            {
                smat[i] = m_h_materials[i];
            }
            m_d_materials.unmap();
        }

        struct DGeometry
        {
            uint32_t m_vertex_offset;
            uint32_t m_index_offset;
            uint32_t m_material_id;
            uint32_t m_padding;
        };

        // build mesh table
        {
            std::vector<DGeometry> instance_geometry_table;
            std::vector<uint32_t> base_instance_table;
            for (size_t i = 0; i < m_base_instances.length(); i++)
            {
                base_instance_table.push_back(instance_geometry_table.size());
                for (size_t k = 0; k < m_base_instances[i].m_geometry_id_ranges.size(); k++)
                {
                    const urange & gid_range = m_base_instances[i].m_geometry_id_ranges[k];
                    for (size_t j = gid_range.m_begin; j < gid_range.m_end; j++)
                    {
                        const auto & geometry = m_geometries[j];
                        DGeometry dgeometry;
                        dgeometry.m_vertex_offset = geometry.m_vbuf_offset;
                        dgeometry.m_index_offset  = geometry.m_ibuf_offset;
                        dgeometry.m_material_id   = geometry.m_material_id;
                        instance_geometry_table.push_back(dgeometry);
                    }
                }
            }

            DGeometry * geometry_table = static_cast<DGeometry *>(m_d_rt_instance_table.map());
            uint32_t * instance_table  = static_cast<uint32_t *>(m_d_rt_geometry_table.map());
            std::memcpy(geometry_table,
                        instance_geometry_table.data(),
                        sizeof(DGeometry) * instance_geometry_table.size());
            std::memcpy(instance_table,
                        base_instance_table.data(),
                        sizeof(uint32_t) * base_instance_table.size());
            m_d_rt_instance_table.unmap();
            m_d_rt_geometry_table.unmap();
        }
    }
};