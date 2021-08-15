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

// --------------------------------

struct SceneGraphLeaf
{
    uint32_t         m_geometry_id       = -1;
    StandardMaterial m_standard_material = {};
    float4x4         m_recent_transform  = glm::identity<float4x4>();
};

struct SceneGraphNode
{
    SceneGraphNode * m_parent                                        = nullptr;
    float4x4         m_transform                                     = glm::identity<float4x4>();
    bool             m_is_dirty                                      = true;
    std::variant<SceneGraphLeaf, std::vector<SceneGraphNode>> m_info = {};

    SceneGraphNode(const bool as_leaf)
    {
        if (as_leaf)
        {
            m_info = SceneGraphLeaf();
        }
        else
        {
            m_info = std::vector<SceneGraphNode>();
        }
    }

    SceneGraphNode() : SceneGraphNode(false) {}

    bool
    is_leaf() const
    {
        return m_info.index() == 0;
    }

    void
    traverse(const std::function<void(const SceneGraphLeaf &)> & func) const
    {
        if (is_leaf())
        {
            const SceneGraphLeaf & leaf_info = std::get<0>(m_info);
            func(leaf_info);
            return;
        }
        else
        {
            const std::vector<SceneGraphNode> & childs = std::get<1>(m_info);
            for (size_t i = 0; i < childs.size(); i++)
            {
                childs[i].traverse(func);
            }
            return;
        }
        assert(false);
    }

    void
    traverse(const std::function<void(SceneGraphLeaf &)> & func)
    {
        if (is_leaf())
        {
            SceneGraphLeaf & leaf_info = std::get<0>(m_info);
            func(leaf_info);
            return;
        }
        else
        {
            std::vector<SceneGraphNode> & childs = std::get<1>(m_info);
            for (size_t i = 0; i < childs.size(); i++)
            {
                childs[i].traverse(func);
            }
            return;
        }
        assert(false);
    }

    void
    update_transform(const float4x4 & current_transform = glm::identity<float4x4>())
    {
        const float4x4 transform = m_transform * current_transform;
        if (is_leaf())
        {
            SceneGraphLeaf & leaf_info   = std::get<0>(m_info);
            leaf_info.m_recent_transform = transform;
            return;
        }
        else
        {
            std::vector<SceneGraphNode> & childs = std::get<1>(m_info);
            for (size_t i = 0; i < childs.size(); i++)
            {
                childs[i].update_transform(transform);
            }
            return;
        }
        assert(false);
    }

    size_t
    get_num_leaves(const std::function<bool(const SceneGraphLeaf &)> & func) const
    {
        if (is_leaf())
        {
            const SceneGraphLeaf & leaf_info = std::get<0>(m_info);
            if (func(leaf_info))
            {
                return 1;
            }
            return 0;
        }
        else
        {
            const std::vector<SceneGraphNode> & childs = std::get<1>(m_info);
            size_t                              sum    = 0;
            for (size_t i = 0; i < childs.size(); i++)
            {
                sum += childs[i].get_num_leaves(func);
            }
            return sum;
        }
        assert(false);
        return 0;
    }
};

struct Geometry
{
    uint32_t m_vbuf_offset  = 0;
    uint32_t m_ibuf_offset  = 0;
    uint32_t m_num_vertices = 0;
    uint32_t m_num_indices  = 0;
    bool     m_is_static    = true;
};

struct Scene
{
    enum class LoadStandardParamEnum
    {
        DiffuseReflectance,
        SpecularReflectance,
        SpecularRoughness
    };

    FpsCamera m_camera;

    const Gp::Device * m_device               = nullptr;
    Gp::Buffer         m_g_vbuf_position      = {};
    Gp::Buffer         m_g_vbuf_compact_info  = {};
    Gp::Buffer         m_g_ibuf               = {};
    Gp::IndexType      m_g_ibuf_index_type    = Gp::IndexType::Uint16;
    Gp::FormatEnum     m_g_vbuf_position_type = Gp::FormatEnum::R32G32B32_SFloat;
    size_t             m_g_vbuf_num_vertices  = 0;
    size_t             m_g_ibuf_num_indices   = 0;

    std::array<Gp::Texture, EngineSetting::MaxNumBindlessTextures>       m_textures;
    size_t                                                               m_num_textures = 0;
    std::array<StandardMaterial, EngineSetting::MaxNumStandardMaterials> m_materials;
    size_t                                                               m_num_materials = 0;
    std::array<Geometry, EngineSetting::MaxNumGeometries>                m_geometries;
    size_t                                                               m_num_geometries = 0;

    Gp::RayTracingBlas m_rt_static_meshes_blas;

    Gp::CommandPool m_graphics_pool;

    SceneGraphNode m_scene_graph_root = SceneGraphNode(false);

    Scene() {}

    Scene(const Gp::Device & device) : m_device(&device)
    {
        m_graphics_pool       = Gp::CommandPool(&device, Gp::CommandQueueType::Transfer);
        m_g_vbuf_position     = Gp::Buffer(m_device,
                                       Gp::BufferUsageEnum::TransferDst,
                                       Gp::MemoryUsageEnum::GpuOnly,
                                       sizeof(float3) * EngineSetting::MaxNumVertices,
                                       "scene_m_g_vbuf_position");
        m_g_vbuf_compact_info = Gp::Buffer(m_device,
                                           Gp::BufferUsageEnum::TransferDst,
                                           Gp::MemoryUsageEnum::GpuOnly,
                                           sizeof(CompactVertex) * EngineSetting::MaxNumVertices,
                                           "scene_m_g_vbuf_compact_info");
        m_g_ibuf              = Gp::Buffer(m_device,
                              Gp::BufferUsageEnum::TransferDst,
                              Gp::MemoryUsageEnum::GpuOnly,
                              Gp::GetSizeInBytes(m_g_ibuf_index_type) * EngineSetting::MaxNumIndices,
                              "scene_m_g_ibuf");
    }

    void
    add_render_object(SceneGraphNode * node, const std::filesystem::path & path, Gp::StagingBufferManager & staging_buffer_manager)
    {
        Assimp::Importer importer;
        const aiScene *  scene =
            importer.ReadFile(path.string(),
                              aiProcess_Triangulate | aiProcess_CalcTangentSpace | aiProcess_GenSmoothNormals |
                                  aiProcess_JoinIdenticalVertices | aiProcess_RemoveRedundantMaterials);
        assert(!node->is_leaf());
        std::vector<SceneGraphNode> & childs = std::get<1>(node->m_info);
        const size_t                  offset = childs.size();
        childs.resize(offset + scene->mNumMeshes);

        // add geometries
        const std::array<size_t, 2> geometries_range = add_geometries(scene, staging_buffer_manager);

        // add materials
        for (size_t i_mat = 0; i_mat < scene->mNumMaterials; i_mat++)
        {
            m_materials[i_mat + m_num_materials] =
                add_required_textures(path, *scene->mMaterials[i_mat], staging_buffer_manager);
        }
        m_num_materials += scene->mNumMaterials;

        // add children in to scene graph
        for (size_t i_child = 0; i_child < scene->mNumMeshes; i_child++)
        {
            SceneGraphLeaf leaf;
            leaf.m_geometry_id              = static_cast<uint32_t>(geometries_range[0] + i_child);
            leaf.m_standard_material        = m_materials[scene->mMeshes[i_child]->mMaterialIndex];
            childs[offset + i_child].m_info = leaf;
            childs[offset + i_child].m_parent    = node;
            childs[offset + i_child].m_transform = glm::identity<float4x4>();
        }
    }

    std::array<size_t, 2>
    add_geometries(const aiScene * scene, Gp::StagingBufferManager & staging_buffer_manager)
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

        Gp::CommandList cmd_list = m_graphics_pool.get_command_list();

        Gp::Fence tmp_fence(staging_buffer_manager.m_device);
        tmp_fence.reset();
        Gp::Buffer staging_buffer(m_device,
                                  Gp::BufferUsageEnum::TransferSrc,
                                  Gp::MemoryUsageEnum::CpuOnly,
                                  vb_positions.size() * sizeof(vb_positions[0]));
        Gp::Buffer staging_buffer2(m_device,
                                   Gp::BufferUsageEnum::TransferSrc,
                                   Gp::MemoryUsageEnum::CpuOnly,
                                   ib.size() * sizeof(ib[0]));
        std::memcpy(staging_buffer.map(), vb_positions.data(), vb_positions.size() * sizeof(vb_positions[0]));
        std::memcpy(staging_buffer2.map(), ib.data(), ib.size() * sizeof(ib[0]));
        staging_buffer.unmap();
        staging_buffer2.unmap();

        cmd_list.begin();
        cmd_list.copy_buffer_region(m_g_vbuf_position,
                                    m_g_vbuf_num_vertices * sizeof(float3),
                                    staging_buffer,
                                    0,
                                    vb_positions.size() * sizeof(vb_positions[0]));
        cmd_list.copy_buffer_region(m_g_ibuf,
                                    m_g_ibuf_num_indices * Gp::GetSizeInBytes(m_g_ibuf_index_type),
                                    staging_buffer2,
                                    0,
                                    ib.size() * sizeof(ib[0]));
        cmd_list.end();
        cmd_list.submit(&tmp_fence);
        tmp_fence.wait();

        vertex_buffer_offset = m_g_vbuf_num_vertices;
        index_buffer_offset  = m_g_ibuf_num_indices;
        for (unsigned int i_mesh = 0; i_mesh < num_meshes; i_mesh++)
        {
            // all information
            const aiMesh * aimesh       = scene->mMeshes[i_mesh];
            const uint32_t num_vertices = static_cast<uint32_t>(aimesh->mNumVertices);
            const uint32_t num_faces    = static_cast<uint32_t>(aimesh->mNumFaces);

            Geometry & model     = m_geometries[m_num_geometries + i_mesh];
            model.m_vbuf_offset  = vertex_buffer_offset;
            model.m_ibuf_offset  = index_buffer_offset;
            model.m_num_indices  = num_faces * 3;
            model.m_num_vertices = num_vertices;
            model.m_is_static    = true;

            vertex_buffer_offset += aimesh->mNumVertices;
            index_buffer_offset += aimesh->mNumFaces * 3;
            vertex_buffer_offset = round_up(vertex_buffer_offset, 32);
            index_buffer_offset  = round_up(index_buffer_offset, 32);
        }

        auto result = std::array<size_t, 2>{ m_num_geometries, m_num_geometries + num_meshes };

        m_g_vbuf_num_vertices += total_num_vertices;
        m_g_ibuf_num_indices += total_num_indices;
        m_num_geometries += num_meshes;

        return result;
    }

    template <size_t NumChannels>
    size_t
    add_texture(const std::array<uint8_t, NumChannels> & v, Gp::StagingBufferManager & staging_buffer_manager)
    {
        // find empty texture slot
        size_t e = 0;
        for (e = 0; e < m_textures.size(); e++)
        {
            if (m_textures[e].is_empty())
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
                            &staging_buffer_manager,
                            float4(),
                            "");
        staging_buffer_manager.submit_all_pending_upload();

        // add to list in manager
        m_textures[e] = std::move(texture);
        return e;
    }

    size_t
    add_texture(const std::filesystem::path & path, const size_t desired_channel, Gp::StagingBufferManager & staging_buffer_manager)
    {
        // find empty texture slot
        size_t e = 0;
        for (e = 0; e < m_textures.size(); e++)
        {
            if (m_textures[e].is_empty())
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
        void * image =
            stbi_load(filepath_str.c_str(), &resolution.x, &resolution.y, nullptr, static_cast<int>(desired_channel));
        assert(image);
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

        m_textures[e] = std::move(texture);
        return e;
    }

    StandardMaterial
    add_required_textures(const std::filesystem::path & path,
                          const aiMaterial &            material,
                          Gp::StagingBufferManager &    staging_buffer_manager)
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

    size_t
    add_texture(const std::filesystem::path & path,
                const aiMaterial &            material,
                const LoadStandardParamEnum   standard_param,
                Gp::StagingBufferManager &    staging_buffer_manager)
    {
        aiTextureType ai_tex_type         = aiTextureType::aiTextureType_NONE;
        const char *  ai_mat_key          = nullptr;
        int           num_desired_channel = 0;
        if (standard_param == LoadStandardParamEnum::DiffuseReflectance)
        {
            ai_tex_type         = aiTextureType::aiTextureType_DIFFUSE;
            ai_mat_key          = AI_MATKEY_COLOR_DIFFUSE;
            num_desired_channel = 4;
        }
        else if (standard_param == LoadStandardParamEnum::SpecularReflectance)
        {
            ai_tex_type         = aiTextureType::aiTextureType_SPECULAR;
            ai_mat_key          = AI_MATKEY_COLOR_DIFFUSE;
            num_desired_channel = 4;
        }
        else if (standard_param == LoadStandardParamEnum::SpecularRoughness)
        {
            ai_tex_type         = aiTextureType::aiTextureType_SHININESS;
            ai_mat_key          = AI_MATKEY_COLOR_DIFFUSE;
            num_desired_channel = 1;
        }

        aiString  tex_name;
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
    build_static_models_blas(Gp::StagingBufferManager & staging_buffer_manager)
    {
        // create vector of description
        const size_t num_static_geoms =
            m_scene_graph_root.get_num_leaves([&](const SceneGraphLeaf & leaf) -> bool {
                const Geometry & geometry = m_geometries[leaf.m_geometry_id];
                if (geometry.m_num_indices > 0 && geometry.m_is_static)
                {
                    return geometry.m_is_static;
                }
            });

        // populate vector of geometry descs
        std::vector<Gp::RayTracingGeometryDesc> static_mesh_descs(num_static_geoms);
        size_t                                  i_static_mesh = 0;
        m_scene_graph_root.traverse([&](const SceneGraphLeaf & leaf_info) {
            const Geometry & geometry = m_geometries[leaf_info.m_geometry_id];
            if (geometry.m_num_indices > 0 && geometry.m_is_static)
            {
                Gp::RayTracingGeometryDesc & geom_desc = static_mesh_descs[i_static_mesh++];
                geom_desc.set_flag(Gp::RayTracingGeometryFlag::Opaque);
                geom_desc.set_index_buffer(m_g_ibuf,
                                           geometry.m_ibuf_offset * Gp::GetSizeInBytes(m_g_ibuf_index_type),
                                           m_g_ibuf_index_type,
                                           geometry.m_num_indices);
                geom_desc.set_vertex_buffer(m_g_vbuf_position,
                                            geometry.m_vbuf_offset * Gp::GetSizeInBytes(m_g_vbuf_position_type),
                                            Gp::FormatEnum::R32G32B32_SFloat,
                                            sizeof(float3),
                                            geometry.m_num_vertices);
            }
        });
        assert(i_static_mesh == num_static_geoms);

        // build blas
        m_rt_static_meshes_blas = Gp::RayTracingBlas(m_device,
                                                     static_mesh_descs.data(),
                                                     static_mesh_descs.size(),
                                                     &staging_buffer_manager,
                                                     "global_blas");

        staging_buffer_manager.submit_all_pending_upload();
    }
};