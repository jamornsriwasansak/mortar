#pragma once

#include "graphicsapi/graphicsapi.h"
#include "render/shared/compactvertex.h"
#include "render/shared/pbrmaterial.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <stb_image.h>

struct MeshBlob
{
    Gp::Buffer          m_vertex_buffer;
    Gp::Buffer          m_index_buffer;
    size_t              m_num_vertices;
    size_t              m_num_indices;
    std::vector<size_t> m_offset_in_vertices;
    std::vector<size_t> m_offset_in_indices;

    size_t
    get_num_vertices(const size_t i_sub_vertex_buffer) const
    {
        return m_offset_in_vertices[i_sub_vertex_buffer + 1] - m_offset_in_vertices[i_sub_vertex_buffer];
    }

    size_t
    get_num_sub_buffers() const
    {
        return m_offset_in_vertices.size() - 1;
    }

    size_t
    get_num_indices(const size_t i_sub_index_buffer) const
    {
        return m_offset_in_indices[i_sub_index_buffer + 1] - m_offset_in_indices[i_sub_index_buffer];
    }
};

struct PbrMesh
{
    size_t m_blob_id;
    size_t m_subblob_id;
    size_t m_material_id;
};

struct AssetManager
{
    std::vector<MeshBlob>                       m_mesh_blobs;
    std::map<std::filesystem::path, size_t> m_mesh_id_from_path;
    std::vector<Gp::Texture>                m_textures;
    std::map<std::filesystem::path, size_t> m_texture_id_from_path;
    Gp::StagingBufferManager *              m_staging_buffer_manager = nullptr;
    Gp::Device *                            m_device                 = nullptr;

    std::vector<PbrMaterial> m_pbr_materials;
    std::vector<PbrMesh>   m_pbr_objects;

    AssetManager() {}

    AssetManager(Gp::Device * device, Gp::StagingBufferManager * staging_buffer_manager)
    : m_device(device), m_staging_buffer_manager(staging_buffer_manager)
    {
        init_empty_texture(staging_buffer_manager);
    }

    bool
    has_texture_path(const std::filesystem::path & path)
    {
        return m_texture_id_from_path.count(path) > 0;
    }

    bool
    has_mesh_path(const std::filesystem::path & path)
    {
        return m_mesh_id_from_path.count(path) > 0;
    }

    size_t
    add_mesh(const std::filesystem::path & path, const bool load_as_a_single_mesh = false, const aiScene * scene = nullptr)
    {
        auto iter = m_mesh_id_from_path.find(path);
        if (iter != m_mesh_id_from_path.end())
        {
            Logger::Warn(__FUNCTION__, " ", path.string(), " has already been added");
            return iter->second;
        }

        auto to_float3 = [&](const aiVector3D & vec3) { return float3(vec3.x, vec3.y, vec3.z); };
        auto to_float2 = [&](const aiVector2D & vec2) { return float2(vec2.x, vec2.y); };

        MeshBlob               result;
        const unsigned int num_meshes = scene->mNumMeshes;
        result.m_offset_in_indices.resize(num_meshes + 1);
        result.m_offset_in_vertices.resize(num_meshes + 1);
        // result.m_material_ids.resize(num_meshes);
        unsigned int ii = 0;
        unsigned int iv = 0;

        for (unsigned int i_mesh = 0; i_mesh < num_meshes; i_mesh++)
        {
            const aiMesh *     aimesh           = scene->mMeshes[i_mesh];
            const unsigned int num_vertices     = aimesh->mNumVertices;
            const unsigned int num_faces        = aimesh->mNumFaces;
            result.m_offset_in_indices[i_mesh]  = ii;
            result.m_offset_in_vertices[i_mesh] = iv;
            iv += num_vertices;
            ii += num_faces * 3;

            // round so the index is align by 32
            iv = round_up(iv, 32);
            ii = round_up(ii, 32);
        }
        result.m_offset_in_indices[num_meshes]  = ii;
        result.m_offset_in_vertices[num_meshes] = iv;
        result.m_num_indices                    = ii;
        result.m_num_vertices                   = iv;

        std::vector<CompactVertex> vertices(iv);
        std::vector<uint32_t>      indices(ii);

        for (unsigned int i_mesh = 0; i_mesh < num_meshes; i_mesh++)
        {
            // set vertex
            const aiMesh *     aimesh          = scene->mMeshes[i_mesh];
            const unsigned int num_vertices    = aimesh->mNumVertices;
            const size_t       i_vertex_offset = result.m_offset_in_vertices[i_mesh];
            for (unsigned int i_vertex = 0; i_vertex < num_vertices; i_vertex++)
            {
                CompactVertex & vertex = vertices[i_vertex_offset + i_vertex];
                vertex.m_position      = to_float3(aimesh->mVertices[i_vertex]);
                vertex.m_normal        = to_float3(aimesh->mNormals[i_vertex]);
                vertex.m_texcoord_x    = aimesh->mTextureCoords[0][i_vertex].x;
                vertex.m_texcoord_y    = aimesh->mTextureCoords[0][i_vertex].y;
            }

            // set faces
            const unsigned int num_faces      = aimesh->mNumFaces;
            const size_t       i_index_offset = result.m_offset_in_indices[i_mesh];
            for (unsigned int i_face = 0; i_face < num_faces; i_face++)
            {
                for (size_t i_vertex = 0; i_vertex < 3; i_vertex++)
                {
                    uint32_t & index = indices[i_index_offset + i_face * 3 + i_vertex];
                    index = static_cast<uint32_t>(aimesh->mFaces[i_face].mIndices[i_vertex]);
                    if (load_as_a_single_mesh)
                    {
                        index += static_cast<uint32_t>(i_vertex_offset);
                    }
                }
            }
        }

        result.m_vertex_buffer = Gp::Buffer(m_device,
                                            Gp::BufferUsageEnum::VertexBuffer | Gp::BufferUsageEnum::StorageBuffer,
                                            Gp::MemoryUsageEnum::GpuOnly,
                                            vertices.size() * sizeof(vertices[0]),
                                            reinterpret_cast<std::byte *>(vertices.data()),
                                            m_staging_buffer_manager,
                                            "vertexbuffer:" + path.string());
        result.m_index_buffer  = Gp::Buffer(m_device,
                                           Gp::BufferUsageEnum::IndexBuffer | Gp::BufferUsageEnum::StorageBuffer,
                                           Gp::MemoryUsageEnum::GpuOnly,
                                           indices.size() * sizeof(indices[0]),
                                           reinterpret_cast<std::byte *>(indices.data()),
                                           m_staging_buffer_manager,
                                           "indexbuffer:" + path.string());
        m_staging_buffer_manager->submit_all_pending_upload();

        const size_t index = m_mesh_blobs.size();
        m_mesh_blobs.emplace_back(std::move(result));
        m_mesh_id_from_path[path] = index;

        return index;
    }

    int2
    add_pbr_object(const std::filesystem::path & path, const bool load_as_a_single_mesh = false)
    {
        // load scene into assimp
        Assimp::Importer importer;
        const aiScene *  scene =
            importer.ReadFile(path.string(),
                              aiProcess_Triangulate | aiProcess_CalcTangentSpace | aiProcess_GenSmoothNormals |
                                  aiProcess_JoinIdenticalVertices | aiProcess_RemoveRedundantMaterials);
        if (!scene)
        {
            Logger::Error<true>(__FUNCTION__ " cannot import mesh from path ", path.string());
        }

        // load all materials
        auto load_assimp_texture = [&](const aiMaterial * material, const aiTextureType ai_tex_type) {
            aiString tex_name;
            if (material->GetTexture(ai_tex_type, 0, &tex_name) == aiReturn_SUCCESS)
            {
                const std::filesystem::path tex_path = path.parent_path() / std::string(tex_name.C_Str());
                return add_texture(tex_path);
            }
            else
            {
                return static_cast<size_t>(0);
            }
        };

        const size_t        num_materials = scene->mNumMaterials;
        std::vector<size_t> pbr_material_id_from_ai_material(scene->mNumMaterials);
        for (unsigned int i_material = 0; i_material < num_materials; i_material++)
        {
            const aiMaterial * material = scene->mMaterials[i_material];

            PbrMaterial pbr_material;
            pbr_material.m_diffuse_tex_id =
                static_cast<int>(load_assimp_texture(material, aiTextureType::aiTextureType_DIFFUSE));
            pbr_material.m_roughness_tex_id =
                static_cast<int>(load_assimp_texture(material, aiTextureType::aiTextureType_DIFFUSE));

            pbr_material_id_from_ai_material[i_material] = m_pbr_materials.size();
            m_pbr_materials.push_back(pbr_material);
        }

        const size_t mesh_id     = add_mesh(path, load_as_a_single_mesh, scene);
        const size_t begin_index = m_pbr_objects.size();
        for (size_t i_submesh = 0; i_submesh < scene->mNumMeshes; i_submesh++)
        {
            PbrMesh pbr_object;
            pbr_object.m_blob_id    = mesh_id;
            pbr_object.m_subblob_id = i_submesh;
            pbr_object.m_material_id =
                pbr_material_id_from_ai_material[scene->mMeshes[i_submesh]->mMaterialIndex];
            m_pbr_objects.push_back(pbr_object);
        }
        const size_t end_index = m_pbr_objects.size();

        return int2(begin_index, end_index);
    }

    size_t
    add_texture(const std::filesystem::path & path)
    {
        auto iter = m_texture_id_from_path.find(path);
        if (iter == m_texture_id_from_path.end())
        {
            const std::string filepath_str = path.string();
            const size_t      index        = m_textures.size();

            // load using stbi
            int2 resolution;
            int  num_channels = 0;
            stbi_set_flip_vertically_on_load(true);
            void * image = stbi_load(filepath_str.c_str(), &resolution.x, &resolution.y, &num_channels, 4);
            std::byte * image_bytes = reinterpret_cast<std::byte *>(image);
            Gp::Texture texture(m_device,
                                Gp::TextureUsageEnum::Sampled,
                                Gp::TextureStateEnum::FragmentShaderVisible,
                                Gp::FormatEnum::R8G8B8A8_UNorm,
                                resolution,
                                image_bytes,
                                m_staging_buffer_manager,
                                float4(),
                                filepath_str);
            m_staging_buffer_manager->submit_all_pending_upload();
            stbi_image_free(image);

            // add to list in manager
            m_textures.emplace_back(std::move(texture));
            m_texture_id_from_path[path] = index;
            return index;
        }
        else
        {
            Logger::Warn(__FUNCTION__, " ", path.string(), " has already been added");
            return iter->second;
        }
    }

private:
    void
    init_empty_texture(Gp::StagingBufferManager * staging_buffer_manager)
    {
        assert(m_textures.size() == 0);
        std::array<uint8_t, 4> default_value = { 0, 0, 0, 0 };
        m_textures.push_back(Gp::Texture(m_device,
                                         Gp::TextureUsageEnum::Sampled,
                                         Gp::TextureStateEnum::FragmentShaderVisible,
                                         Gp::FormatEnum::R8G8B8A8_UNorm,
                                         int2(1, 1),
                                         reinterpret_cast<std::byte *>(default_value.data()),
                                         staging_buffer_manager,
                                         float4(),
                                         "texture_manager_empty_texture"));
    }
};