#pragma once

#include "core/camera.h"
#include "core/ste/stevector.h"
#include "engine_setting.h"
#include "importer/ai_mesh_importer.h"
#include "importer/img_importer.h"
#include "rhi/rhi.h"
#include "shaders/shared/bindless_table.h"
#include "shaders/shared/compact_vertex.h"
#include "shaders/shared/standard_material.h"
#include "shaders/shared/types.h"
//
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <filesystem>
#include <scene_graph.h>

struct SceneGeometry
{
    BufferSizeT m_vbuf_base_idx = 0;
    BufferSizeT m_ibuf_base_idx = 0;
    BufferSizeT m_num_vertices  = 0;
    BufferSizeT m_num_indices   = 0;
    BufferSizeT m_material_idx  = 0;
    bool        m_is_updatable  = false;
};

struct SceneBaseInstance
{
    std::vector<urange32_t> m_geometry_id_ranges = {};
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
    Rhi::Device & m_device;

    // command pool
    Rhi::CommandPool m_transfer_cmd_pool;

    static constexpr Rhi::IndexType  m_ibuf_index_type    = Rhi::GetIndexType<VertexIndexT>();
    static constexpr Rhi::FormatEnum m_vbuf_position_type = Rhi::GetVertexType<float3>();

    // device position, packed and index information
    Rhi::Buffer m_d_vbuf_position = {};
    Rhi::Buffer m_d_vbuf_packed   = {};
    Rhi::Buffer m_d_ibuf          = {};
    size_t      m_num_vertices    = 0;
    size_t      m_num_indices     = 0;

    // device & host textures and materials
    std::vector<Rhi::Texture>               m_d_textures;
    Rhi::Buffer                             m_d_materials = {};
    std::vector<StandardMaterial>           m_h_materials;
    std::map<std::filesystem::path, size_t> m_texture_id_from_path;

    // device & host lookup table for geometry & instance
    // look up offset into geometry table based on instance index
    Rhi::Buffer m_d_base_instance_table           = {};
    Rhi::Buffer m_d_geometry_table                = {};
    size_t      m_num_base_instance_table_entries = 0;
    size_t      m_num_geometry_table_entries      = 0;

    // device & host blas (requires update)
    std::vector<Rhi::RayTracingBlas> m_rt_blases;
    Rhi::RayTracingTlas              m_rt_tlas;

    // camera
    FpsCamera m_camera;

    // scene graph
    SceneGraphNode                 m_scene_graph_root = SceneGraphNode(false);
    std::vector<SceneGeometry>     m_geometries;
    std::vector<SceneBaseInstance> m_base_instances;

    SceneResource(Rhi::Device & device)
    : m_device(device),
      m_transfer_cmd_pool("scene_resource_transfer_cmd_pool", device, Rhi::CommandQueueType::Transfer)
    {
        // index buffer vertex buffer
        m_d_vbuf_position = Rhi::Buffer("scene_m_d_vbuf_position",
                                        m_device,
                                        Rhi::BufferUsageEnum::TransferDst | Rhi::BufferUsageEnum::StorageBuffer |
                                            Rhi::BufferUsageEnum::VertexBuffer | Rhi::BufferUsageEnum::RayTracingAccelStructBufferInput,
                                        Rhi::MemoryUsageEnum::GpuOnly,
                                        sizeof(float3) * EngineSetting::MaxNumVertices);
        m_d_vbuf_packed   = Rhi::Buffer("scene_m_d_vbuf_packed",
                                      m_device,
                                      Rhi::BufferUsageEnum::TransferDst | Rhi::BufferUsageEnum::StorageBuffer |
                                          Rhi::BufferUsageEnum::VertexBuffer,
                                      Rhi::MemoryUsageEnum::GpuOnly,
                                      sizeof(CompactVertex) * EngineSetting::MaxNumVertices);
        m_d_ibuf          = Rhi::Buffer("scene_m_d_ibuf",
                               m_device,
                               Rhi::BufferUsageEnum::TransferDst | Rhi::BufferUsageEnum::StorageBuffer |
                                   Rhi::BufferUsageEnum::IndexBuffer | Rhi::BufferUsageEnum::RayTracingAccelStructBufferInput,
                               Rhi::MemoryUsageEnum::GpuOnly,
                               Rhi::GetSizeInBytes(m_ibuf_index_type) * EngineSetting::MaxNumIndices);

        // materials
        m_d_materials = Rhi::Buffer("scene_m_d_materials",
                                    m_device,
                                    Rhi::BufferUsageEnum::TransferDst | Rhi::BufferUsageEnum::StorageBuffer,
                                    Rhi::MemoryUsageEnum::GpuOnly,
                                    sizeof(StandardMaterial) * EngineSetting::MaxNumStandardMaterials);

        // geometry table and geometry offset table
        m_d_base_instance_table =
            Rhi::Buffer("scene_m_d_base_instance_table",
                        m_device,
                        Rhi::BufferUsageEnum::TransferDst | Rhi::BufferUsageEnum::StorageBuffer,
                        Rhi::MemoryUsageEnum::GpuOnly,
                        sizeof(BaseInstanceTableEntry) * EngineSetting::MaxNumGeometryOffsetTableEntry);
        m_d_geometry_table =
            Rhi::Buffer("scene_m_d_geometry_table",
                        m_device,
                        Rhi::BufferUsageEnum::TransferDst | Rhi::BufferUsageEnum::StorageBuffer,
                        Rhi::MemoryUsageEnum::GpuOnly,
                        sizeof(GeometryTableEntry) * EngineSetting::MaxNumGeometryTableEntry);
    }

    urange32_t
    add_geometries(const std::filesystem::path & path, Rhi::StagingBufferManager & staging_buffer_manager)
    {
        std::optional<AiScene>      ai_scene = AiScene::ReadScene(path);
        std::vector<AiGeometryInfo> geometry_infos =
            ai_scene->get_geometry_infos(std::numeric_limits<VertexIndexT>::max());

        // load all materials (and necessary textures)
        const unsigned int material_offset = m_h_materials.size();
        for (size_t i_mat = 0; i_mat < ai_scene->m_ai_scene->mNumMaterials; i_mat++)
        {
            StandardMaterial mat =
                add_standard_material(path, *ai_scene->m_ai_scene->mMaterials[i_mat], staging_buffer_manager);
            m_h_materials.push_back(mat);
        }

        // prepare information host vertex buffers allocation and index buffer
        size_t              num_total_vertices = 0;
        size_t              num_total_indices  = 0;
        std::vector<size_t> vertices_base_idxs(geometry_infos.size());
        std::vector<size_t> indices_base_idxs(geometry_infos.size());
        for (size_t i_geometry_info = 0; i_geometry_info < geometry_infos.size(); i_geometry_info++)
        {
            vertices_base_idxs[i_geometry_info] = num_total_vertices;
            indices_base_idxs[i_geometry_info]  = num_total_indices;
            num_total_vertices +=
                round_up(geometry_infos[i_geometry_info].m_dst_num_vertices, static_cast<size_t>(32));
            num_total_indices +=
                round_up(geometry_infos[i_geometry_info].m_dst_num_indices, static_cast<size_t>(32));
        }

        // allocate host vertex buffers and index buffer
        std::vector<float3>        vb_positions1(num_total_vertices);
        std::vector<CompactVertex> vb_packed1(num_total_vertices);
        std::vector<VertexIndexT>  ib1(num_total_indices);

        // prepare the result
        const urange32_t geometries_range(m_geometries.size(), m_geometries.size() + geometry_infos.size());
        m_geometries.resize(geometries_range.m_end);

        for (size_t i_geometry_info = 0; i_geometry_info < geometry_infos.size(); i_geometry_info++)
        {
            const AiGeometryInfo &   geometry_info     = geometry_infos[i_geometry_info];
            const uint32_t           vertices_base_idx = vertices_base_idxs[i_geometry_info];
            const uint32_t           indices_base_idx  = indices_base_idxs[i_geometry_info];
            std::span<float3>        span_vb_positions(vb_positions1.begin() + vertices_base_idx,
                                                geometry_info.m_dst_num_vertices);
            std::span<CompactVertex> span_vb_packed(vb_packed1.begin() + vertices_base_idx,
                                                    geometry_info.m_dst_num_vertices);
            std::span<VertexIndexT> span_ib(ib1.begin() + indices_base_idx, geometry_info.m_dst_num_indices);

            ai_scene->write_geometry_info(&span_vb_positions, &span_vb_packed, &span_ib, geometry_info);

            SceneGeometry & model = m_geometries[i_geometry_info + geometries_range.m_begin];

            // TODO:: have to be offset if pushed more than one mesh
            model.m_vbuf_base_idx = vertices_base_idx;
            model.m_ibuf_base_idx = indices_base_idx;
            model.m_num_indices   = geometry_info.m_dst_num_indices;
            model.m_num_vertices  = geometry_info.m_dst_num_vertices;
            model.m_is_updatable  = true;
            model.m_material_idx  = material_offset + geometry_info.m_src_material_index;
        }

        Rhi::CommandList cmd_list = m_transfer_cmd_pool.get_command_list();

        Rhi::Buffer staging_buffer("scene_staging_buffer_vb",
                                   m_device,
                                   Rhi::BufferUsageEnum::TransferSrc,
                                   Rhi::MemoryUsageEnum::CpuOnly,
                                   vb_positions1.size() * sizeof(vb_positions1[0]));
        Rhi::Buffer staging_buffer2("scene_staging_buffer_ib",
                                    m_device,
                                    Rhi::BufferUsageEnum::TransferSrc,
                                    Rhi::MemoryUsageEnum::CpuOnly,
                                    ib1.size() * sizeof(ib1[0]));
        Rhi::Buffer staging_buffer3("scene_staging_buffer_vb_packed",
                                    m_device,
                                    Rhi::BufferUsageEnum::TransferSrc,
                                    Rhi::MemoryUsageEnum::CpuOnly,
                                    vb_packed1.size() * sizeof(vb_packed1[0]));

        std::memcpy(staging_buffer.map(), vb_positions1.data(), vb_positions1.size() * sizeof(vb_positions1[0]));
        std::memcpy(staging_buffer2.map(), ib1.data(), ib1.size() * sizeof(ib1[0]));
        std::memcpy(staging_buffer3.map(), vb_packed1.data(), vb_packed1.size() * sizeof(vb_packed1[0]));
        staging_buffer.unmap();
        staging_buffer2.unmap();
        staging_buffer3.unmap();

        static_assert(Rhi::GetSizeInBytes(m_vbuf_position_type) == sizeof(vb_positions1[0]));
        static_assert(Rhi::GetSizeInBytes(m_ibuf_index_type) == sizeof(ib1[0]));

        cmd_list.begin();
        cmd_list.copy_buffer_region(m_d_vbuf_position,
                                    m_num_vertices * Rhi::GetSizeInBytes(m_vbuf_position_type),
                                    staging_buffer,
                                    0,
                                    vb_positions1.size() * sizeof(vb_positions1[0]));
        cmd_list.copy_buffer_region(m_d_ibuf,
                                    m_num_indices * Rhi::GetSizeInBytes(m_ibuf_index_type),
                                    staging_buffer2,
                                    0,
                                    ib1.size() * sizeof(ib1[0]));
        cmd_list.copy_buffer_region(m_d_vbuf_packed,
                                    m_num_vertices * sizeof(CompactVertex),
                                    staging_buffer3,
                                    0,
                                    vb_packed1.size() * sizeof(vb_packed1[0]));
        cmd_list.end();

        Rhi::Fence tmp_fence("fence upload " + path.string(), staging_buffer_manager.m_device);
        tmp_fence.reset();
        cmd_list.submit(&tmp_fence);
        tmp_fence.wait();

        m_num_vertices += vb_positions1.size();
        m_num_indices += ib1.size();

        return geometries_range;
    }

    size_t
    add_base_instance(const std::span<urange32_t> & geometry_ranges)
    {
        SceneBaseInstance base_instance;
        base_instance.m_geometry_id_ranges =
            std::vector<urange32_t>(geometry_ranges.begin(), geometry_ranges.end());
        m_base_instances.push_back(base_instance);
        return m_base_instances.size() - 1;
    }

    StandardMaterial
    add_standard_material(const std::filesystem::path & path,
                          const aiMaterial &            ai_material,
                          Rhi::StagingBufferManager &   staging_buffer_manager)
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

    void
    set_material(uint32_t *                    blob,
                 StandardMaterial *            material,
                 const std::filesystem::path & path,
                 const aiMaterial &            ai_material,
                 const aiTextureType           ai_tex_type,
                 const char *                  ai_mat_key_0,
                 int                           ai_mat_key_1,
                 int                           ai_mat_key_2,
                 const int                     num_desired_channels,
                 Rhi::StagingBufferManager &   staging_buffer_manager)
    {
        aiString  tex_name;
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
        auto q = m_texture_id_from_path.find(path);

        if (q != m_texture_id_from_path.end())
        {
            return q->second;
        }

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
        Rhi::Texture texture(filepath_str,
                             staging_buffer_manager.m_device,
                             Rhi::TextureUsageEnum::Sampled,
                             Rhi::TextureStateEnum::FragmentShaderVisible,
                             format_enum,
                             resolution,
                             image_bytes,
                             &staging_buffer_manager,
                             float4());
        staging_buffer_manager.submit_all_pending_upload();
        stbi_image_free(image);

        // emplace back
        m_d_textures.emplace_back(std::move(texture));
        const size_t tex_id          = m_d_textures.size() - 1;
        m_texture_id_from_path[path] = tex_id;

        return tex_id;
    }

    void
    commit(const SceneDesc & scene_desc, Rhi::StagingBufferManager & staging_buffer_manager)
    {
        // create blas for all base instance
        {
            std::vector<Rhi::RayTracingGeometryDesc> geom_descs;
            m_rt_blases.resize(m_base_instances.size());

            for (size_t i_binst = 0; i_binst < m_base_instances.size(); i_binst++)
            {
                SceneBaseInstance & base_instance = m_base_instances[i_binst];

                // prepare descs
                geom_descs.clear();

                // populate geometry descs
                bool is_updatable = false;
                for (size_t i_geom = 0; i_geom < base_instance.m_geometry_id_ranges.size(); i_geom++)
                {
                    const urange32_t & gid_range = base_instance.m_geometry_id_ranges[i_geom];
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
                                                   geometry.m_ibuf_base_idx * Rhi::GetSizeInBytes(m_ibuf_index_type),
                                                   m_ibuf_index_type,
                                                   geometry.m_num_indices);
                        geom_desc.set_vertex_buffer(m_d_vbuf_position,
                                                    geometry.m_vbuf_base_idx * Rhi::GetSizeInBytes(m_vbuf_position_type),
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
                m_rt_blases[i_binst] =
                    Rhi::RayTracingBlas("blas_" + std::to_string(i_binst), m_device, geom_descs, hint, &staging_buffer_manager);
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
            m_rt_tlas = Rhi::RayTracingTlas("ray_tracing_tlas", m_device, instances, &staging_buffer_manager);
            staging_buffer_manager.submit_all_pending_upload();
        }

        Rhi::CommandList cmd_list = m_transfer_cmd_pool.get_command_list();
        cmd_list.begin();

        // build material buffer
        Rhi::Buffer staging_buffer("scene_staging_buffer_material",
                                   m_device,
                                   Rhi::BufferUsageEnum::TransferSrc,
                                   Rhi::MemoryUsageEnum::CpuOnly,
                                   m_h_materials.size() * sizeof(m_h_materials[0]));
        {
            std::memcpy(staging_buffer.map(),
                        m_h_materials.data(),
                        m_h_materials.size() * sizeof(m_h_materials[0]));
            staging_buffer.unmap();
            cmd_list.copy_buffer_region(m_d_materials, 0, staging_buffer, 0, m_h_materials.size() * sizeof(m_h_materials[0]));
        }

        // build mesh table
        Rhi::Buffer staging_buffer2 = {};
        Rhi::Buffer staging_buffer3 = {};
        {
            std::vector<GeometryTableEntry>     geometry_table;
            std::vector<BaseInstanceTableEntry> base_instance_table;
            for (size_t i = 0; i < m_base_instances.size(); i++)
            {
                BaseInstanceTableEntry base_instance_entry;
                base_instance_entry.m_geometry_table_index_base = geometry_table.size();
                base_instance_table.push_back(base_instance_entry);
                for (size_t k = 0; k < m_base_instances[i].m_geometry_id_ranges.size(); k++)
                {
                    const urange32_t & gid_range = m_base_instances[i].m_geometry_id_ranges[k];
                    for (size_t j = gid_range.m_begin; j < gid_range.m_end; j++)
                    {
                        const auto &       geometry = m_geometries[j];
                        GeometryTableEntry geometry_entry;
                        geometry_entry.m_vertex_base_idx = geometry.m_vbuf_base_idx;
                        geometry_entry.m_index_base_idx  = geometry.m_ibuf_base_idx;
                        geometry_entry.m_material_idx    = geometry.m_material_idx;
                        geometry_table.push_back(geometry_entry);
                    }
                }
            }

            staging_buffer2 = Rhi::Buffer("scene_staging_buffer_geometry_table",
                                          m_device,
                                          Rhi::BufferUsageEnum::TransferSrc,
                                          Rhi::MemoryUsageEnum::CpuOnly,
                                          sizeof(GeometryTableEntry) * geometry_table.size());
            staging_buffer3 = Rhi::Buffer("scene_staging_buffer_base_instance",
                                          m_device,
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

        Rhi::Fence fence("scene_commit_fence", m_device);
        fence.reset();
        cmd_list.submit(&fence);
        fence.wait();
    }
};