#pragma once

#include "graphicsapi/graphicsapi.h"
#include "render/shared/compactvertex.h"
#include "render/shared/pbremissive.h"
#include "render/shared/pbrmaterial.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <stb_image.h>

struct PbrMesh
{
    size_t m_vertex_buffer_id;
    size_t m_index_buffer_id;
    size_t m_vertex_buffer_offset;
    size_t m_index_buffer_offset;
    size_t m_num_vertices;
    size_t m_num_indices;
};

struct PbrObject
{
    int m_mesh_id     = -1;
    int m_material_id = -1;
    int m_emissive_id = -1;
};

struct VertexBuffer
{
    Gp::Buffer m_buffer;
    size_t     m_num_vertices;
};

struct IndexBuffer
{
    Gp::Buffer    m_buffer;
    size_t        m_num_indices;
    Gp::IndexType m_type;
};

struct AssetPool
{
    enum class LoadPbrParamEnum
    {
        DiffuseReflectance,
        SpecularReflectance,
        SpecularRoughness
    };

    // pool
    std::vector<VertexBuffer>               m_vertex_buffers;
    std::vector<IndexBuffer>                m_index_buffers;
    std::vector<PbrMaterial>                m_pbr_materials;
    std::vector<PbrEmissive>                m_pbr_emissives;
    std::vector<PbrMesh>                    m_pbr_meshes;
    std::vector<Gp::Texture>                m_textures;
    std::map<std::filesystem::path, size_t> m_texture_id_from_path;
    Gp::StagingBufferManager *              m_staging_buffer_manager = nullptr;
    Gp::Device *                            m_device                 = nullptr;

    AssetPool() {}

    AssetPool(Gp::Device * device, Gp::StagingBufferManager * staging_buffer_manager)
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
        unsigned int       ii         = 0;
        unsigned int       iv         = 0;

        int2 result;
        result.x = m_pbr_meshes.size();
        result.y = m_pbr_meshes.size() + num_meshes;
        m_pbr_meshes.resize(m_pbr_meshes.size() + num_meshes);

        for (unsigned int i_mesh = 0; i_mesh < num_meshes; i_mesh++)
        {
            const aiMesh *     aimesh                              = scene->mMeshes[i_mesh];
            const unsigned int num_vertices                        = aimesh->mNumVertices;
            const unsigned int num_faces                           = aimesh->mNumFaces;
            m_pbr_meshes[result.x + i_mesh].m_vertex_buffer_offset = iv;
            m_pbr_meshes[result.x + i_mesh].m_index_buffer_offset  = ii;
            m_pbr_meshes[result.x + i_mesh].m_num_vertices         = num_vertices;
            m_pbr_meshes[result.x + i_mesh].m_num_indices          = num_faces * 3;
            iv += num_vertices;
            ii += num_faces * 3;

            // round so the index is align by 32
            iv = round_up(iv, 32);
            ii = round_up(ii, 32);
        }

        std::vector<CompactVertex> vertices(iv);
        std::vector<uint32_t>      indices(ii);

        for (unsigned int i_mesh = 0; i_mesh < num_meshes; i_mesh++)
        {
            // set vertex
            const aiMesh *     aimesh       = scene->mMeshes[i_mesh];
            const unsigned int num_vertices = aimesh->mNumVertices;
            const size_t i_vertex_offset = m_pbr_meshes[result.x + i_mesh].m_vertex_buffer_offset;
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
            const size_t i_index_offset  = m_pbr_meshes[result.x + i_mesh].m_index_buffer_offset;
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

        m_staging_buffer_manager->submit_all_pending_upload();

        VertexBuffer vb;
        vb.m_buffer       = Gp::Buffer(m_device,
                                 Gp::BufferUsageEnum::VertexBuffer | Gp::BufferUsageEnum::StorageBuffer,
                                 Gp::MemoryUsageEnum::GpuOnly,
                                 vertices.size() * sizeof(vertices[0]),
                                 reinterpret_cast<std::byte *>(vertices.data()),
                                 m_staging_buffer_manager,
                                 "vertexbuffer:" + path.string());
        vb.m_num_vertices = vertices.size();

        IndexBuffer ib;
        ib.m_buffer      = Gp::Buffer(m_device,
                                 Gp::BufferUsageEnum::IndexBuffer | Gp::BufferUsageEnum::StorageBuffer,
                                 Gp::MemoryUsageEnum::GpuOnly,
                                 indices.size() * sizeof(indices[0]),
                                 reinterpret_cast<std::byte *>(indices.data()),
                                 m_staging_buffer_manager,
                                 "indexbuffer:" + path.string());
        ib.m_num_indices = indices.size();
        ib.m_type        = Gp::IndexType::Uint32;

        m_vertex_buffers.push_back(std::move(vb));
        m_index_buffers.push_back(std::move(ib));

        for (unsigned int i_mesh = 0; i_mesh < num_meshes; i_mesh++)
        {
            m_pbr_meshes[result.x + i_mesh].m_vertex_buffer_id = m_vertex_buffers.size() - 1;
            m_pbr_meshes[result.x + i_mesh].m_index_buffer_id  = m_index_buffers.size() - 1;
        }

        return result;
    }

    int
    add_pbr_material(const PbrMaterial & material)
    {
        m_pbr_materials.push_back(material);
        return m_pbr_materials.size() - 1;
    }

    int
    add_pbr_emissive(const PbrEmissive & emissive)
    {
        m_pbr_emissives.push_back(emissive);
        return m_pbr_emissives.size() - 1;
    }

    int
    add_ai_texture(const std::filesystem::path & path, const aiMaterial * material, const LoadPbrParamEnum pbr_param)
    {
        aiTextureType ai_tex_type         = aiTextureType::aiTextureType_NONE;
        const char *  ai_mat_key          = nullptr;
        int           num_desired_channel = 0;
        if (pbr_param == LoadPbrParamEnum::DiffuseReflectance)
        {
            ai_tex_type         = aiTextureType::aiTextureType_DIFFUSE;
            ai_mat_key          = AI_MATKEY_COLOR_DIFFUSE;
            num_desired_channel = 4;
        }
        else if (pbr_param == LoadPbrParamEnum::SpecularReflectance)
        {
            ai_tex_type         = aiTextureType::aiTextureType_SPECULAR;
            ai_mat_key          = AI_MATKEY_COLOR_DIFFUSE;
            num_desired_channel = 4;
        }
        else if (pbr_param == LoadPbrParamEnum::SpecularRoughness)
        {
            ai_tex_type         = aiTextureType::aiTextureType_SHININESS;
            ai_mat_key          = AI_MATKEY_COLOR_DIFFUSE;
            num_desired_channel = 1;
        }

        aiString  tex_name;
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

    std::vector<PbrObject>
    add_pbr_meshes(const std::filesystem::path & path,
                   const bool                    load_as_a_single_mesh = false,
                   const bool                    load_material         = true)
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
        std::vector<size_t> pbr_material_id_from_ai_material(scene->mNumMaterials);
        if (load_material)
        {
            for (unsigned int i_material = 0; i_material < scene->mNumMaterials; i_material++)
            {
                const aiMaterial * material = scene->mMaterials[i_material];

                PbrMaterial pbr_material;
                pbr_material.m_diffuse_tex_id =
                    add_ai_texture(path, material, LoadPbrParamEnum::DiffuseReflectance);
                pbr_material.m_specular_tex_id =
                    add_ai_texture(path, material, LoadPbrParamEnum::SpecularReflectance);
                pbr_material.m_roughness_tex_id =
                    add_ai_texture(path, material, LoadPbrParamEnum::SpecularRoughness);

                pbr_material_id_from_ai_material[i_material] = m_pbr_materials.size();
                m_pbr_materials.push_back(pbr_material);
            }
        }

        std::vector<PbrObject> result;
        const int2             pbr_mesh_range = add_mesh(path, load_as_a_single_mesh, scene);
        for (size_t i_mesh = 0; i_mesh < scene->mNumMeshes; i_mesh++)
        {
            PbrObject object;
            object.m_mesh_id = pbr_mesh_range.x + i_mesh;
            if (load_material)
            {
                object.m_material_id = pbr_material_id_from_ai_material[scene->mMeshes[i_mesh]->mMaterialIndex];
            }
            result.push_back(object);
        }

        return result;
    }

    template <size_t NumChannels>
    size_t
    add_constant_texture(const std::array<uint8_t, NumChannels> & v)
    {
        // load using stbi
        int2 resolution;
        static_assert(NumChannels == 4 || NumChannels == 1, "num channels must be 4 or 1");
        Gp::FormatEnum format_enum = v.size() == 4 ? Gp::FormatEnum::R8G8B8A8_UNorm : Gp::FormatEnum::R8_UNorm;
        Gp::Texture texture(m_device,
                            Gp::TextureUsageEnum::Sampled,
                            Gp::TextureStateEnum::FragmentShaderVisible,
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
            const size_t      index        = m_textures.size();

            // load using stbi
            int2 resolution;
            stbi_set_flip_vertically_on_load(true);
            void * image = stbi_load(filepath_str.c_str(), &resolution.x, &resolution.y, nullptr, desired_channel);
            std::byte * image_bytes = reinterpret_cast<std::byte *>(image);
            assert(desired_channel == 4 || desired_channel == 1);
            Gp::FormatEnum format_enum =
                desired_channel == 4 ? Gp::FormatEnum::R8G8B8A8_UNorm : Gp::FormatEnum::R8_UNorm;
            Gp::Texture texture(m_device,
                                Gp::TextureUsageEnum::Sampled,
                                Gp::TextureStateEnum::FragmentShaderVisible,
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