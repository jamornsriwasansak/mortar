#pragma once

#include "rhi/rhi.h"
//
#include "core/camera.h"
#include "core/ste/stevector.h"
#include "loader/img_loader.h"
//
#include "engine_setting.h"
#include "shaders/shared/bindless_table.h"
#include "shaders/shared/compact_vertex.h"
#include "shaders/shared/standard_material.h"
//
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <filesystem>
#include <scene_graph.h>

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
    const Rhi::Device * m_device = nullptr;

    // command pool
    Rhi::CommandPool m_transfer_cmd_pool;

    static constexpr Rhi::IndexType m_ibuf_index_type     = Rhi::GetIndexType<uint16_t>();
    static constexpr Rhi::FormatEnum m_vbuf_position_type = Rhi::GetVertexType<float3>();

    // device position, packed and index information
    Rhi::Buffer m_d_vbuf_position = {};
    Rhi::Buffer m_d_vbuf_packed   = {};
    Rhi::Buffer m_d_ibuf          = {};
    size_t m_num_vertices         = 0;
    size_t m_num_indices          = 0;

    // device & host textures and materials
    Std::FsVector<Rhi::Texture, EngineSetting::MaxNumBindlessTextures> m_d_textures;
    Rhi::Buffer m_d_materials = {};
    Std::FsVector<StandardMaterial, EngineSetting::MaxNumStandardMaterials> m_h_materials;

    // device & host lookup table for geometry & instance
    // look up offset into geometry table based on instance index
    Rhi::Buffer m_d_base_instance_table      = {};
    Rhi::Buffer m_d_geometry_table           = {};
    size_t m_num_base_instance_table_entries = 0;
    size_t m_num_geometry_table_entries      = 0;

    // device & host blas (requires update)
    std::vector<Rhi::RayTracingBlas> m_rt_blases;
    Rhi::RayTracingTlas m_rt_tlas;

    // camera
    FpsCamera m_camera;

    // scene graph
    SceneGraphNode m_scene_graph_root = SceneGraphNode(false);
    Std::FsVector<SceneGeometry, EngineSetting::MaxNumGeometries> m_geometries;
    Std::FsVector<SceneBaseInstance, EngineSetting::MaxNumBaseInstances> m_base_instances;

    // TODO:: too complicated for now
    // std::vector<SceneGraphNode> m_scene_static_instances;
    // std::vector<SceneGraphNode> m_scene_dynamic_instances;

    SceneResource() {}

    SceneResource(const Rhi::Device & device) : m_device(&device)
    {
        m_transfer_cmd_pool = Rhi::CommandPool(&device, Rhi::CommandQueueType::Graphics);

        // index buffer vertex buffer
        m_d_vbuf_position = Rhi::Buffer(m_device,
                                        Rhi::BufferUsageEnum::TransferDst | Rhi::BufferUsageEnum::StorageBuffer |
                                            Rhi::BufferUsageEnum::VertexBuffer,
                                        Rhi::MemoryUsageEnum::GpuOnly,
                                        sizeof(float3) * EngineSetting::MaxNumVertices,
                                        "scene_m_d_vbuf_position");
        m_d_vbuf_packed   = Rhi::Buffer(m_device,
                                      Rhi::BufferUsageEnum::TransferDst | Rhi::BufferUsageEnum::StorageBuffer |
                                          Rhi::BufferUsageEnum::VertexBuffer,
                                      Rhi::MemoryUsageEnum::GpuOnly,
                                      sizeof(CompactVertex) * EngineSetting::MaxNumVertices,
                                      "scene_m_d_vbuf_packed");
        m_d_ibuf          = Rhi::Buffer(m_device,
                               Rhi::BufferUsageEnum::TransferDst | Rhi::BufferUsageEnum::StorageBuffer |
                                   Rhi::BufferUsageEnum::IndexBuffer,
                               Rhi::MemoryUsageEnum::GpuOnly,
                               Rhi::GetSizeInBytes(m_ibuf_index_type) * EngineSetting::MaxNumIndices,
                               "scene_m_d_ibuf");

        // materials
        m_d_materials = Rhi::Buffer(m_device,
                                    Rhi::BufferUsageEnum::TransferDst | Rhi::BufferUsageEnum::StorageBuffer,
                                    Rhi::MemoryUsageEnum::GpuOnly,
                                    sizeof(StandardMaterial) * EngineSetting::MaxNumStandardMaterials,
                                    "scene_m_d_materials");

        // geometry table and geometry offset table
        m_d_base_instance_table =
            Rhi::Buffer(m_device,
                        Rhi::BufferUsageEnum::TransferDst | Rhi::BufferUsageEnum::StorageBuffer,
                        Rhi::MemoryUsageEnum::GpuOnly,
                        sizeof(BaseInstanceTableEntry) * EngineSetting::MaxNumGeometryOffsetTableEntry,
                        "scene_m_d_base_instance_table");
        m_d_geometry_table =
            Rhi::Buffer(m_device,
                        Rhi::BufferUsageEnum::TransferDst | Rhi::BufferUsageEnum::StorageBuffer,
                        Rhi::MemoryUsageEnum::GpuOnly,
                        sizeof(GeometryTableEntry) * EngineSetting::MaxNumGeometryTableEntry,
                        "scene_m_d_geometry_table");
    }

    urange
    add_geometries(const std::filesystem::path & path, Rhi::StagingBufferManager & staging_buffer_manager)
    {
        Assimp::Importer importer;
        const aiScene * scene = importer.ReadFile(path.string(), aiProcess_GenNormals);

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
        unsigned int total_num_vertices = 0;
        unsigned int total_num_indices  = 0;
        {
            for (unsigned int i_mesh = 0; i_mesh < num_meshes; i_mesh++)
            {
                const aiMesh * aimesh = scene->mMeshes[i_mesh];
                total_num_vertices += aimesh->mNumVertices;

                for (unsigned int i_ai_face = 0; i_ai_face < aimesh->mNumFaces; i_ai_face++)
                {
                    const unsigned int num_ai_indices = aimesh->mFaces[i_ai_face].mNumIndices;
                    const unsigned int num_indices    = (num_ai_indices - 2) * 3;
                    total_num_indices += num_indices;
                }

                // round so the index is align by 32
                total_num_vertices = round_up(total_num_vertices, 32);
                total_num_indices  = round_up(total_num_indices, 32);
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
                if (num_vertices > std::numeric_limits<uint16_t>::max())
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
                    float3 snormal         = to_float3(aimesh->mNormals[i_vertex]);
                    const float slen       = length(snormal);
                    if (slen == 0.0f)
                    {
                        snormal = float3(0.0f, 0.0f, 1.0f);
                    }
                    else
                    {
                        snormal = snormal / slen;
                    }

                    vertex.set_snormal(snormal);
                    vertex.m_texcoord.x = aimesh->mTextureCoords[0][i_vertex].x;
                    vertex.m_texcoord.y = aimesh->mTextureCoords[0][i_vertex].y;
                }

                // set faces
                unsigned int next_index_buffer_offset = index_buffer_offset;
                for (unsigned int i_face = 0; i_face < num_faces; i_face++)
                {
                    const uint16_t ai_index0 = aimesh->mFaces[i_face].mIndices[0];
                    for (unsigned int i_fan = 0; i_fan < aimesh->mFaces[i_face].mNumIndices - 2; i_fan++)
                    {
                        // convert triangle fan -> triangles
                        uint16_t & index0 = ib[next_index_buffer_offset + 0];
                        uint16_t & index1 = ib[next_index_buffer_offset + 1];
                        uint16_t & index2 = ib[next_index_buffer_offset + 2];

                        const uint16_t ai_index1 = aimesh->mFaces[i_face].mIndices[i_fan + 1];
                        const uint16_t ai_index2 = aimesh->mFaces[i_face].mIndices[i_fan + 2];

                        index0 = ai_index0;
                        index1 = ai_index1;
                        index2 = ai_index2;

                        next_index_buffer_offset += 3;
                    }
                }

                SceneGeometry model;
                model.m_vbuf_offset  = vertex_buffer_offset;
                model.m_ibuf_offset  = index_buffer_offset;
                model.m_num_indices  = next_index_buffer_offset - index_buffer_offset;
                model.m_num_vertices = num_vertices;
                model.m_is_updatable = true;
                model.m_material_id  = material_offset + aimesh->mMaterialIndex;
                m_geometries.push_back(model);

                vertex_buffer_offset += aimesh->mNumVertices;
                vertex_buffer_offset = round_up(vertex_buffer_offset, 32);
                index_buffer_offset  = next_index_buffer_offset;
                index_buffer_offset  = round_up(index_buffer_offset, 32);
            }
            assert(index_buffer_offset == total_num_indices);

            Rhi::CommandList cmd_list = m_transfer_cmd_pool.get_command_list();

            Rhi::Buffer staging_buffer(m_device,
                                       Rhi::BufferUsageEnum::TransferSrc,
                                       Rhi::MemoryUsageEnum::CpuOnly,
                                       vb_positions.size() * sizeof(vb_positions[0]));
            Rhi::Buffer staging_buffer2(m_device,
                                        Rhi::BufferUsageEnum::TransferSrc,
                                        Rhi::MemoryUsageEnum::CpuOnly,
                                        ib.size() * sizeof(ib[0]));
            Rhi::Buffer staging_buffer3(m_device,
                                        Rhi::BufferUsageEnum::TransferSrc,
                                        Rhi::MemoryUsageEnum::CpuOnly,
                                        vb_packed_info.size() * sizeof(vb_packed_info[0]));

            std::memcpy(staging_buffer.map(), vb_positions.data(), vb_positions.size() * sizeof(vb_positions[0]));
            std::memcpy(staging_buffer2.map(), ib.data(), ib.size() * sizeof(ib[0]));
            std::memcpy(staging_buffer3.map(),
                        vb_packed_info.data(),
                        vb_packed_info.size() * sizeof(vb_packed_info[0]));
            staging_buffer.unmap();
            staging_buffer2.unmap();
            staging_buffer3.unmap();

            static_assert(Rhi::GetSizeInBytes(m_vbuf_position_type) == sizeof(vb_positions[0]));
            static_assert(Rhi::GetSizeInBytes(m_ibuf_index_type) == sizeof(ib[0]));

            cmd_list.begin();
            cmd_list.copy_buffer_region(m_d_vbuf_position,
                                        m_num_vertices * Rhi::GetSizeInBytes(m_vbuf_position_type),
                                        staging_buffer,
                                        0,
                                        vb_positions.size() * sizeof(vb_positions[0]));
            cmd_list.copy_buffer_region(m_d_ibuf,
                                        m_num_indices * Rhi::GetSizeInBytes(m_ibuf_index_type),
                                        staging_buffer2,
                                        0,
                                        ib.size() * sizeof(ib[0]));
            cmd_list.copy_buffer_region(m_d_vbuf_packed,
                                        m_num_vertices * sizeof(CompactVertex),
                                        staging_buffer3,
                                        0,
                                        vb_packed_info.size() * sizeof(vb_packed_info[0]));
            cmd_list.end();

            Rhi::Fence tmp_fence(staging_buffer_manager.m_device);
            tmp_fence.reset();
            cmd_list.submit(&tmp_fence);
            tmp_fence.wait();

            vertex_buffer_offset = m_num_vertices;
            index_buffer_offset  = m_num_indices;

            m_num_vertices += total_num_vertices;
            m_num_indices += total_num_indices;
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
                          const aiMaterial & ai_material,
                          Rhi::StagingBufferManager & staging_buffer_manager)
    {
        StandardMaterial standard_material;
        set_material(&standard_material.m_diffuse_tex_id,
                     &standard_material,
                     path,
                     ai_material,
                     aiTextureType::aiTextureType_DIFFUSE,
                     AI_MATKEY_COLOR_DIFFUSE,
                     4,
                     staging_buffer_manager);
        set_material(&standard_material.m_specular_tex_id,
                     &standard_material,
                     path,
                     ai_material,
                     aiTextureType::aiTextureType_SPECULAR,
                     AI_MATKEY_COLOR_SPECULAR,
                     4,
                     staging_buffer_manager);
        set_material(&standard_material.m_roughness_tex_id,
                     &standard_material,
                     path,
                     ai_material,
                     aiTextureType::aiTextureType_SHININESS,
                     AI_MATKEY_SHININESS,
                     1,
                     staging_buffer_manager);
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

    void
    set_material(uint32_t * blob,
                 StandardMaterial * material,
                 const std::filesystem::path & path,
                 const aiMaterial & ai_material,
                 const aiTextureType ai_tex_type,
                 const char * ai_mat_key_0,
                 int ai_mat_key_1,
                 int ai_mat_key_2,
                 const int num_desired_channels,
                 Rhi::StagingBufferManager & staging_buffer_manager)
    {
        aiString tex_name;
        aiColor4D color;
        if (ai_material.GetTexture(ai_tex_type, 0, &tex_name) == aiReturn_SUCCESS)
        {
            const std::filesystem::path tex_path = path.parent_path() / std::string(tex_name.C_Str());
            *blob = add_texture(tex_path, num_desired_channels, staging_buffer_manager);
            assert((*blob & (1 << 24)) == 0);
        }
        else if (aiGetMaterialColor(&ai_material, ai_mat_key_0, ai_mat_key_1, ai_mat_key_2, &color) == aiReturn_SUCCESS)
        {
            if (num_desired_channels == 1)
            {
                *blob = material->encode_r(color.r);
            }
            else if (num_desired_channels == 4 || num_desired_channels == 3 || num_desired_channels == 2)
            {
                *blob = material->encode_rgb(float3(color.r, color.g, color.b));
            }
            assert((*blob & (1 << 24)) != 0);
        }
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

        Rhi::CommandList cmd_list = m_transfer_cmd_pool.get_command_list();
        cmd_list.begin();

        // build material buffer
        Rhi::Buffer staging_buffer(m_device,
                                   Rhi::BufferUsageEnum::TransferSrc,
                                   Rhi::MemoryUsageEnum::CpuOnly,
                                   m_h_materials.length() * sizeof(m_h_materials[0]));
        {
            std::memcpy(staging_buffer.map(),
                        m_h_materials.data(),
                        m_h_materials.length() * sizeof(m_h_materials[0]));
            staging_buffer.unmap();
            cmd_list.copy_buffer_region(m_d_materials,
                                        0,
                                        staging_buffer,
                                        0,
                                        m_h_materials.length() * sizeof(m_h_materials[0]));
        }

        // build mesh table
        Rhi::Buffer staging_buffer2 = {};
        Rhi::Buffer staging_buffer3 = {};
        {
            std::vector<GeometryTableEntry> geometry_table;
            std::vector<BaseInstanceTableEntry> base_instance_table;
            for (size_t i = 0; i < m_base_instances.length(); i++)
            {
                BaseInstanceTableEntry base_instance_entry;
                base_instance_entry.m_geometry_entry_offset = geometry_table.size();
                base_instance_table.push_back(base_instance_entry);
                for (size_t k = 0; k < m_base_instances[i].m_geometry_id_ranges.size(); k++)
                {
                    const urange & gid_range = m_base_instances[i].m_geometry_id_ranges[k];
                    for (size_t j = gid_range.m_begin; j < gid_range.m_end; j++)
                    {
                        const auto & geometry = m_geometries[j];
                        GeometryTableEntry geometry_entry;
                        geometry_entry.m_vertex_offset = geometry.m_vbuf_offset;
                        geometry_entry.m_index_offset  = geometry.m_ibuf_offset;
                        geometry_entry.m_material_id   = geometry.m_material_id;
                        geometry_table.push_back(geometry_entry);
                    }
                }
            }

            staging_buffer2 = Rhi::Buffer(m_device,
                                          Rhi::BufferUsageEnum::TransferSrc,
                                          Rhi::MemoryUsageEnum::CpuOnly,
                                          sizeof(GeometryTableEntry) * geometry_table.size());
            staging_buffer3 = Rhi::Buffer(m_device,
                                          Rhi::BufferUsageEnum::TransferSrc,
                                          Rhi::MemoryUsageEnum::CpuOnly,
                                          sizeof(BaseInstanceTableEntry) * base_instance_table.size());
            std::memcpy(staging_buffer2.map(),
                        geometry_table.data(),
                        geometry_table.size() * sizeof(geometry_table[0]));
            std::memcpy(staging_buffer3.map(),
                        base_instance_table.data(),
                        base_instance_table.size() * sizeof(base_instance_table[0]));
            staging_buffer2.unmap();
            staging_buffer3.unmap();
            cmd_list.copy_buffer_region(m_d_geometry_table,
                                        0,
                                        staging_buffer2,
                                        0,
                                        sizeof(GeometryTableEntry) * geometry_table.size());
            cmd_list.copy_buffer_region(m_d_base_instance_table,
                                        0,
                                        staging_buffer3,
                                        0,
                                        sizeof(BaseInstanceTableEntry) * base_instance_table.size());
            m_num_base_instance_table_entries = base_instance_table.size();
            m_num_geometry_table_entries      = geometry_table.size();
        }
        cmd_list.end();

        Rhi::Fence fence(m_device);
        fence.reset();
        cmd_list.submit(&fence);
        fence.wait();
    }
};