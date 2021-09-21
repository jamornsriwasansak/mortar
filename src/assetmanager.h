#pragma once

#include "render/common/compact_vertex.h"
#include "render/common/standard_emissive_ref.h"
#include "render/common/standard_material_ref.h"
#include "rhi/rhi.h"
#include "scene.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <stb_image.h>

struct SharedVertexBuffer
{
    Rhi::Buffer m_buffer;
    size_t m_num_vertices = 0;
    size_t m_ref_count    = -1;
};

struct SharedIndexBuffer
{
    Rhi::Buffer m_buffer;
    Rhi::IndexType m_type;
    size_t m_num_indices = 0;
    size_t m_ref_count   = -1;
};


struct AssetPool
{
    enum class LoadStandardParamEnum
    {
        DiffuseReflectance,
        SpecularReflectance,
        SpecularRoughness
    };

    using VertexBuffer = SharedVertexBuffer;
    using IndexBuffer  = SharedIndexBuffer;

    // pool
    std::vector<VertexBuffer> m_vertex_buffers;
    std::vector<IndexBuffer> m_index_buffers;
    std::vector<StandardMaterial> m_standard_materials;
    std::vector<StandardEmissive> m_standard_emissives;
    std::vector<StandardMesh> m_standard_meshes;
    std::vector<Rhi::Texture> m_textures;
    std::map<std::filesystem::path, size_t> m_texture_id_from_path;
    Rhi::StagingBufferManager * m_staging_buffer_manager = nullptr;
    Rhi::Device * m_device                               = nullptr;

    AssetPool() {}

    AssetPool(Rhi::Device * device, Rhi::StagingBufferManager * staging_buffer_manager)
    : m_device(device), m_staging_buffer_manager(staging_buffer_manager)
    {
        init_empty_texture(staging_buffer_manager);
    }

    int2
    add_mesh(const std::filesystem::path & path, const bool load_as_a_single_mesh = false, const aiScene * scene = nullptr)
    {
        auto to_float3 = [&](const aiVector3D & vec3) { return float3(vec3.x, vec3.y, vec3.z); };
        auto to_float2 = [&](const aiVector2D & vec2) { return float2(vec2.x, vec2.y); };

        const unsigned int num_meshes = scene->mNumMeshes;
        unsigned int ii               = 0;
        unsigned int iv               = 0;

        int2 result;
        result.x = m_standard_meshes.size();
        result.y = m_standard_meshes.size() + num_meshes;
        m_standard_meshes.resize(m_standard_meshes.size() + num_meshes);

        for (unsigned int i_mesh = 0; i_mesh < num_meshes; i_mesh++)
        {
            const aiMesh * aimesh                                       = scene->mMeshes[i_mesh];
            const unsigned int num_vertices                             = aimesh->mNumVertices;
            const unsigned int num_faces                                = aimesh->mNumFaces;
            m_standard_meshes[result.x + i_mesh].m_vertex_buffer_offset = iv;
            m_standard_meshes[result.x + i_mesh].m_index_buffer_offset  = ii;
            m_standard_meshes[result.x + i_mesh].m_num_vertices         = num_vertices;
            m_standard_meshes[result.x + i_mesh].m_num_indices          = num_faces * 3;
            iv += num_vertices;
            ii += num_faces * 3;

            // round so the index is align by 32
            iv = round_up(iv, 32);
            ii = round_up(ii, 32);
        }

        std::vector<CompactVertex> vertices(iv);
        std::vector<uint32_t> indices(ii);

        for (unsigned int i_mesh = 0; i_mesh < num_meshes; i_mesh++)
        {
            // set vertex
            const aiMesh * aimesh           = scene->mMeshes[i_mesh];
            const unsigned int num_vertices = aimesh->mNumVertices;
            const size_t i_vertex_offset = m_standard_meshes[result.x + i_mesh].m_vertex_buffer_offset;
            for (unsigned int i_vertex = 0; i_vertex < num_vertices; i_vertex++)
            {
                CompactVertex & vertex = vertices[i_vertex_offset + i_vertex];
                vertex.m_position      = to_float3(aimesh->mVertices[i_vertex]);
                vertex.m_snormal       = to_float3(aimesh->mNormals[i_vertex]);
                vertex.m_texcoord_x    = aimesh->mTextureCoords[0][i_vertex].x;
                vertex.m_texcoord_y    = aimesh->mTextureCoords[0][i_vertex].y;
            }

            // set faces
            const unsigned int num_faces = aimesh->mNumFaces;
            const size_t i_index_offset = m_standard_meshes[result.x + i_mesh].m_index_buffer_offset;
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

        VertexBuffer vb;
        vb.m_buffer       = Rhi::Buffer(m_device,
                                  Rhi::BufferUsageEnum::VertexBuffer | Rhi::BufferUsageEnum::StorageBuffer,
                                  Rhi::MemoryUsageEnum::GpuOnly,
                                  vertices.size() * sizeof(vertices[0]),
                                  reinterpret_cast<std::byte *>(vertices.data()),
                                  m_staging_buffer_manager,
                                  "vertexbuffer:" + path.string());
        vb.m_num_vertices = vertices.size();

        IndexBuffer ib;
        ib.m_buffer      = Rhi::Buffer(m_device,
                                  Rhi::BufferUsageEnum::IndexBuffer | Rhi::BufferUsageEnum::StorageBuffer,
                                  Rhi::MemoryUsageEnum::GpuOnly,
                                  indices.size() * sizeof(indices[0]),
                                  reinterpret_cast<std::byte *>(indices.data()),
                                  m_staging_buffer_manager,
                                  "indexbuffer:" + path.string());
        ib.m_num_indices = indices.size();
        ib.m_type        = Rhi::IndexType::Uint32;

        m_staging_buffer_manager->submit_all_pending_upload();

        m_vertex_buffers.push_back(std::move(vb));
        m_index_buffers.push_back(std::move(ib));

        for (unsigned int i_mesh = 0; i_mesh < num_meshes; i_mesh++)
        {
            m_standard_meshes[result.x + i_mesh].m_vertex_buffer_id = m_vertex_buffers.size() - 1;
            m_standard_meshes[result.x + i_mesh].m_index_buffer_id  = m_index_buffers.size() - 1;
        }

        return result;
    }

    int
    add_standard_material(const StandardMaterial & material)
    {
        m_standard_materials.push_back(material);
        return m_standard_materials.size() - 1;
    }

    int
    add_standard_emissive(const StandardEmissive & emissive)
    {
        m_standard_emissives.push_back(emissive);
        return m_standard_emissives.size() - 1;
    }

    int
    add_ai_texture(const std::filesystem::path & path, const aiMaterial * material, const LoadStandardParamEnum standard_param)
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
            ai_mat_key          = AI_MATKEY_COLOR_DIFFUSE;
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
        if (material->GetTexture(ai_tex_type, 0, &tex_name) == aiReturn_SUCCESS)
        {
            const std::filesystem::path tex_path = path.parent_path() / std::string(tex_name.C_Str());
            return add_texture_from_path(tex_path, num_desired_channel);
        }
        else if (aiGetMaterialColor(material, ai_mat_key, 0, 0, &color) == aiReturn_SUCCESS)
        {
            if (num_desired_channel == 1)
            {
                uint8_t r = static_cast<uint8_t>(color.r * 256);
                return add_constant_texture(std::array<uint8_t, 1>{ r });
            }
            else if (num_desired_channel == 4)
            {
                uint8_t r = static_cast<uint8_t>(color.r * 256);
                uint8_t g = static_cast<uint8_t>(color.g * 256);
                uint8_t b = static_cast<uint8_t>(color.b * 256);
                uint8_t a = static_cast<uint8_t>(color.a * 256);
                return add_constant_texture(std::array<uint8_t, 4>{ r, g, b, a });
            }
        }

        return static_cast<size_t>(0);
    };

    std::vector<StandardObject>
    add_standard_meshes(const std::filesystem::path & path,
                        const bool load_as_a_single_mesh = false,
                        const bool load_material         = true)
    {
        StopWatch sw;
        sw.reset();
        // load scene into assimp
        Assimp::Importer importer;
        const aiScene * scene =
            importer.ReadFile(path.string(),
                              aiProcess_Triangulate | aiProcess_CalcTangentSpace | aiProcess_GenSmoothNormals |
                                  aiProcess_JoinIdenticalVertices | aiProcess_RemoveRedundantMaterials);
        if (!scene)
        {
            Logger::Error<true>(__FUNCTION__ " cannot import mesh from path ", path.string());
        }
        Logger::Info(__FUNCTION__,
                     " assimp loaded : ",
                     path.string(),
                     ", elapse : ",
                     sw.time_milli_sec(),
                     "ms");
        sw.reset();

        // load all materials
        std::vector<size_t> standard_material_id_from_ai_material(scene->mNumMaterials);
        if (load_material)
        {
            for (unsigned int i_material = 0; i_material < scene->mNumMaterials; i_material++)
            {
                const aiMaterial * material = scene->mMaterials[i_material];

                StandardMaterial standard_material;
                standard_material.m_diffuse_tex_id =
                    add_ai_texture(path, material, LoadStandardParamEnum::DiffuseReflectance);
                standard_material.m_specular_tex_id =
                    add_ai_texture(path, material, LoadStandardParamEnum::SpecularReflectance);
                standard_material.m_roughness_tex_id =
                    add_ai_texture(path, material, LoadStandardParamEnum::SpecularRoughness);

                standard_material_id_from_ai_material[i_material] = m_standard_materials.size();
                m_standard_materials.push_back(standard_material);
            }
        }

        std::vector<StandardObject> result;
        const int2 standard_mesh_range = add_mesh(path, load_as_a_single_mesh, scene);
        for (size_t i_mesh = 0; i_mesh < scene->mNumMeshes; i_mesh++)
        {
            StandardObject object;
            object.m_mesh_id = standard_mesh_range.x + i_mesh;
            if (load_material)
            {
                object.m_material_id =
                    standard_material_id_from_ai_material[scene->mMeshes[i_mesh]->mMaterialIndex];
            }
            result.push_back(object);
        }

        Logger::Info(__FUNCTION__,
                     " repacking data from : ",
                     path.string(),
                     ", elapse : ",
                     sw.time_milli_sec(),
                     "ms");

        return result;
    }

    template <size_t NumChannels>
    size_t
    add_constant_texture(const std::array<uint8_t, NumChannels> & v)
    {
        // load using stbi
        static_assert(NumChannels == 4 || NumChannels == 1, "num channels must be 4 or 1");
        Rhi::FormatEnum format_enum = v.size() == 4 ? Rhi::FormatEnum::R8G8B8A8_UNorm : Rhi::FormatEnum::R8_UNorm;
        Rhi::Texture texture(m_device,
                             Rhi::TextureUsageEnum::Sampled,
                             Rhi::TextureStateEnum::FragmentShaderVisible,
                             format_enum,
                             int2(1, 1),
                             reinterpret_cast<const std::byte *>(v.data()),
                             m_staging_buffer_manager,
                             float4(),
                             "");
        m_staging_buffer_manager->submit_all_pending_upload();

        // add to list in manager
        m_textures.emplace_back(std::move(texture));
        return m_textures.size() - 1;
    }

    size_t
    add_texture_from_path(const std::filesystem::path & path, const size_t desired_channel)
    {
        auto iter = m_texture_id_from_path.find(path);
        if (iter == m_texture_id_from_path.end())
        {
            const std::string filepath_str = path.string();
            const size_t index             = m_textures.size();

            // load using stbi
            int2 resolution;
            stbi_set_flip_vertically_on_load(true);
            void * image = stbi_load(filepath_str.c_str(), &resolution.x, &resolution.y, nullptr, desired_channel);
            std::byte * image_bytes = reinterpret_cast<std::byte *>(image);
            assert(desired_channel == 4 || desired_channel == 1);
            Rhi::FormatEnum format_enum =
                desired_channel == 4 ? Rhi::FormatEnum::R8G8B8A8_UNorm : Rhi::FormatEnum::R8_UNorm;
            Rhi::Texture texture(m_device,
                                 Rhi::TextureUsageEnum::Sampled,
                                 Rhi::TextureStateEnum::FragmentShaderVisible,
                                 format_enum,
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
    init_empty_texture(Rhi::StagingBufferManager * staging_buffer_manager)
    {
        assert(m_textures.size() == 0);
        std::array<uint8_t, 4> default_value = { 0, 0, 0, 0 };
        m_textures.push_back(Rhi::Texture(m_device,
                                          Rhi::TextureUsageEnum::Sampled,
                                          Rhi::TextureStateEnum::FragmentShaderVisible,
                                          Rhi::FormatEnum::R8G8B8A8_UNorm,
                                          int2(1, 1),
                                          reinterpret_cast<std::byte *>(default_value.data()),
                                          staging_buffer_manager,
                                          float4(),
                                          "texture_manager_empty_texture"));
    }
};