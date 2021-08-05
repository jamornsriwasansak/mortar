#pragma once

#include "common/camera.h"
#include "graphicsapi/graphicsapi.h"
#include "loader/img_loader.h"
#include "render/common/engine_setting.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <filesystem>

struct StandardMesh
{
    size_t m_vertex_buffer_id;
    size_t m_index_buffer_id;
    size_t m_vertex_buffer_offset;
    size_t m_index_buffer_offset;
    size_t m_num_vertices;
    size_t m_num_indices;
};

struct StandardObject
{
    int m_mesh_id     = -1;
    int m_material_id = -1;
    int m_emissive_id = -1;
};

struct SharedVertexBuffer
{
    Gp::Buffer m_buffer;
    size_t     m_num_vertices = 0;
    size_t     m_ref_count    = -1;
};

struct SharedIndexBuffer
{
    Gp::Buffer    m_buffer;
    Gp::IndexType m_type;
    size_t        m_num_indices = 0;
    size_t        m_ref_count   = -1;
};

struct SharedTexture
{
    Gp::Texture m_texture;
    size_t      m_ref_count = -1;
};

struct SharedModel
{
    size_t m_vertex_buffer_id     = -1;
    size_t m_index_buffer_id      = -1;
    size_t m_vertex_buffer_offset = 0;
    size_t m_index_buffer_offset  = 0;
    size_t m_num_vertices         = 0;
    size_t m_num_indices          = 0;
    size_t m_ref_count            = -1;
};

struct Model
{
    uint32_t           m_vbuf_offset  = 0;
    uint32_t           m_ibuf_offset  = 0;
    uint32_t           m_num_vertices = 0;
    uint32_t           m_num_indices  = 0;
    Gp::RayTracingBlas m_rt_blas      = {};
};

struct Scene
{
    FpsCamera m_camera;

    Gp::Buffer m_g_vbuf_position;
    Gp::Buffer m_g_vbuf_compact_info;
    Gp::Buffer m_g_ibuf;

    // TODO:: remove the pointer and work on vbuf & ibuf mem manager
    // size_t m_g_vbuf_pointer;
    // size_t m_g_ibuf_pointer;
    size_t m_g_vbuf_max_vertices = 0;
    size_t m_g_ibuf_max_indices  = 0;

    std::array<SharedTexture, EngineSetting::MaxNumBindlessTextures>     m_textures;
    std::array<StandardMaterial, EngineSetting::MaxNumStandardMaterials> m_materials;
    std::array<Model, EngineSetting::MaxNumModels>                       m_models;

    Gp::CommandPool m_graphics_pool;

    Scene() {}

    Scene(const Gp::Device & device)
    {
        m_graphics_pool = Gp::CommandPool(&device, Gp::CommandQueueType::Transfer);
    }

    void
    add_models(const std::filesystem::path & path, Gp::StagingBufferManager & staging_buffer_manager)
    {
        assert(m_g_vbuf_max_vertices == 0);
        assert(m_g_ibuf_max_indices == 0);

        Assimp::Importer importer;
        const aiScene *  scene =
            importer.ReadFile(path.string(),
                              aiProcess_Triangulate | aiProcess_CalcTangentSpace | aiProcess_GenSmoothNormals |
                                  aiProcess_JoinIdenticalVertices | aiProcess_RemoveRedundantMaterials);
        set_gbuf_with_assimp_scene(scene, staging_buffer_manager, path.string());
    }

    template <size_t NumChannels>
    size_t
    add_constant_texture(const std::array<uint8_t, NumChannels> & v, Gp::StagingBufferManager & staging_buffer_manager)
    {
        // find empty texture slot
        size_t e = 0;
        for (e = 0; e < m_textures.size(); e++)
        {
            if (m_textures[e].m_ref_count == -1)
            {
                break;
            }
        }
        assert(e != m_textures.size());
        if (e == m_textures.size())
        {
            return -1;
        }

        // load using stbi
        static_assert(NumChannels == 4 || NumChannels == 1, "num channels must be 4 or 1");
        Gp::FormatEnum format_enum = v.size() == 4 ? Gp::FormatEnum::R8G8B8A8_UNorm : Gp::FormatEnum::R8_UNorm;
        Gp::Texture texture(staging_buffer_manager.m_device,
                            Gp::TextureUsageEnum::Sampled,
                            Gp::TextureStateEnum::FragmentShaderVisible,
                            format_enum,
                            int2(1, 1),
                            reinterpret_cast<const std::byte *>(v.data()),
                            staging_buffer_manager,
                            float4(),
                            "");
        staging_buffer_manager.submit_all_pending_upload();

        // add to list in manager
        m_textures[e].m_texture   = std::move(texture);
        m_textures[e].m_ref_count = 0;
        return e;
    }

    size_t
    add_texture(const std::filesystem::path & path, const size_t desired_channel, Gp::StagingBufferManager & staging_buffer_manager)
    {
        // find empty texture slot
        size_t e = 0;
        for (e = 0; e < m_textures.size(); e++)
        {
            if (m_textures[e].m_ref_count == -1)
            {
                break;
            }
        }
        assert(e != m_textures.size());
        if (e == m_textures.size())
        {
            return -1;
        }

        const std::string filepath_str = path.string();

        // load using stbi
        int2 resolution;
        stbi_set_flip_vertically_on_load(true);
        void * image = stbi_load(filepath_str.c_str(), &resolution.x, &resolution.y, nullptr, desired_channel);
        std::byte * image_bytes = reinterpret_cast<std::byte *>(image);
        assert(desired_channel == 4 || desired_channel == 1);
        Gp::FormatEnum format_enum =
            desired_channel == 4 ? Gp::FormatEnum::R8G8B8A8_UNorm : Gp::FormatEnum::R8_UNorm;
        Gp::Texture texture(staging_buffer_manager.m_device,
                            Gp::TextureUsageEnum::Sampled,
                            Gp::TextureStateEnum::FragmentShaderVisible,
                            format_enum,
                            resolution,
                            image_bytes,
                            &staging_buffer_manager,
                            float4(),
                            filepath_str);
        staging_buffer_manager.submit_all_pending_upload();
        stbi_image_free(image);

        m_textures[e].m_texture   = std::move(texture);
        m_textures[e].m_ref_count = 0;
        return e;
    }

    void
    remove_texture(const size_t texture_id)
    {
        SharedTexture & stex = m_textures[texture_id];
        assert(stex.m_ref_count == 0);
        stex.m_texture   = Gp::Texture();
        stex.m_ref_count = -1;
    }

    size_t
    add_material()
    {
        return -1;
    }

private:
    void
    set_gbuf_with_assimp_scene(const aiScene *            scene,
                               Gp::StagingBufferManager & staging_buffer_manager,
                               const std::string &        name)
    {
        auto to_float3 = [&](const aiVector3D & vec3) { return float3(vec3.x, vec3.y, vec3.z); };
        auto to_float2 = [&](const aiVector2D & vec2) { return float2(vec2.x, vec2.y); };

        const unsigned int num_meshes = scene->mNumMeshes;

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

        std::vector<float3>        vb_positions(total_num_vertices);
        std::vector<CompactVertex> vb_compact_info(total_num_vertices);
        std::vector<uint16_t>      ib(total_num_indices);

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
                CompactVertex & vertex = vb_compact_info[vertex_buffer_offset + i_vertex];
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

        m_g_vbuf_position     = Gp::Buffer(staging_buffer_manager.m_device,
                                       Gp::BufferUsageEnum::VertexBuffer | Gp::BufferUsageEnum::StorageBuffer |
                                           Gp::BufferUsageEnum::TransferDst,
                                       Gp::MemoryUsageEnum::GpuOnly,
                                       vb_positions.size() * sizeof(vb_positions[0]),
                                       nullptr,
                                       nullptr,
                                       "vertexbuffer_" + name);
        m_g_vbuf_compact_info = Gp::Buffer(staging_buffer_manager.m_device,
                                           Gp::BufferUsageEnum::VertexBuffer | Gp::BufferUsageEnum::StorageBuffer |
                                               Gp::BufferUsageEnum::TransferDst,
                                           Gp::MemoryUsageEnum::GpuOnly,
                                           vb_compact_info.size() * sizeof(vb_compact_info[0]),
                                           nullptr,
                                           nullptr,
                                           "vertexbuffer_" + name);
        m_g_ibuf              = Gp::Buffer(staging_buffer_manager.m_device,
                              Gp::BufferUsageEnum::IndexBuffer | Gp::BufferUsageEnum::StorageBuffer |
                                  Gp::BufferUsageEnum::TransferDst,
                              Gp::MemoryUsageEnum::GpuOnly,
                              ib.size() * sizeof(ib[0]),
                              nullptr,
                              nullptr,
                              "indexbuffer_" + name);

        Gp::Buffer staging_buffer(staging_buffer_manager.m_device,
                                  Gp::BufferUsageEnum::TransferSrc,
                                  Gp::MemoryUsageEnum::CpuOnly,
                                  vb_positions.size() * sizeof(vb_positions[0]));

        Gp::CommandList cmd_list = m_graphics_pool.get_command_list();
        cmd_list.begin();
        cmd_list.update_buffer_subresources(m_g_vbuf_position,
                                            0,
                                            reinterpret_cast<std::byte *>(vb_positions.data()),
                                            vb_positions.size() * sizeof(vb_positions[0]),
                                            staging_buffer);
        cmd_list.end();
        cmd_list.submit(nullptr);

        m_g_vbuf_max_vertices = total_num_vertices;
        m_g_ibuf_max_indices  = total_num_indices;

        vertex_buffer_offset = 0;
        index_buffer_offset  = 0;
        for (unsigned int i_mesh = 0; i_mesh < num_meshes; i_mesh++)
        {
            // all information
            const aiMesh * aimesh       = scene->mMeshes[i_mesh];
            const uint32_t num_vertices = static_cast<uint32_t>(aimesh->mNumVertices);
            const uint32_t num_faces    = static_cast<uint32_t>(aimesh->mNumFaces);

            Model & model        = m_models[i_mesh];
            model.m_vbuf_offset  = vertex_buffer_offset;
            model.m_ibuf_offset  = index_buffer_offset;
            model.m_num_indices  = num_faces * 3;
            model.m_num_vertices = num_vertices;

            vertex_buffer_offset += aimesh->mNumVertices;
            index_buffer_offset += aimesh->mNumFaces * 3;
            vertex_buffer_offset = round_up(vertex_buffer_offset, 32);
            index_buffer_offset  = round_up(index_buffer_offset, 32);
        }
    }
};