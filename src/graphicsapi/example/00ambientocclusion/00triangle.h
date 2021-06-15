#pragma once

#include "../../sharedsignature/pbrmaterial.h"
#include "../../sharedsignature/sharedvertex.h"
#include "00constantbuffer.h"
#include "00ssaoparam.h"
#include "common/camera.h"
#include "graphicsapi/dxa/dxa.h"
#include "graphicsapi/vka/vka.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <stb_image.h>

template <typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

namespace Gp = Vka;

struct Mesh
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

struct PbrObject
{
    size_t m_mesh_id;
    size_t m_submesh_id;
    size_t m_material_id;
};

struct AssetManager
{
    std::vector<Mesh>                       m_meshes;
    std::map<std::filesystem::path, size_t> m_mesh_id_from_path;
    std::vector<Gp::Texture>                m_textures;
    std::map<std::filesystem::path, size_t> m_texture_id_from_path;
    Gp::StagingBufferManager *              m_staging_buffer_manager;
    Gp::Device *                            m_device;

    std::vector<PbrMaterial> m_pbr_materials;
    std::vector<PbrObject>   m_pbr_objects;

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

        Mesh               result;
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

        std::vector<HlslVertexIn> vertices(iv);
        std::vector<uint32_t>     indices(ii);

        for (unsigned int i_mesh = 0; i_mesh < num_meshes; i_mesh++)
        {
            // set vertex
            const aiMesh *     aimesh          = scene->mMeshes[i_mesh];
            const unsigned int num_vertices    = aimesh->mNumVertices;
            const size_t       i_vertex_offset = result.m_offset_in_vertices[i_mesh];
            for (unsigned int i_vertex = 0; i_vertex < num_vertices; i_vertex++)
            {
                HlslVertexIn & vertex = vertices[i_vertex_offset + i_vertex];
                vertex.m_position     = to_float3(aimesh->mVertices[i_vertex]);
                vertex.m_normal       = to_float3(aimesh->mNormals[i_vertex]);
                vertex.m_texcoord_x   = aimesh->mTextureCoords[0][i_vertex].x;
                vertex.m_texcoord_y   = aimesh->mTextureCoords[0][i_vertex].y;
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

        const size_t index = m_meshes.size();
        m_meshes.emplace_back(std::move(result));
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
            PbrObject pbr_object;
            pbr_object.m_mesh_id    = mesh_id;
            pbr_object.m_submesh_id = i_submesh;
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

int
mainnn()
{
    int2 m_resolution = int2(1920, 1080);

    // create glfw state
    Window m_window = Window("Mortar 2.0", m_resolution);

    // create entry
    const bool                      is_debug = false;
    Gp::Entry                       factory(m_window, is_debug);
    std::vector<Gp::PhysicalDevice> devices = factory.get_graphics_devices();
    Gp::Device                      device(devices[0]);

    Gp::Swapchain swapchain(&device, m_window, "main_swapchain");

    // create
    size_t                 num_flights = 2;
    std::vector<Gp::Fence> fences(num_flights);
    for (int i = 0; i < num_flights; i++)
    {
        fences[i] = Gp::Fence(&device, "fence" + std::to_string(i));
    }

    // create render target manager and render target
    std::vector<Gp::Texture> swapchain_color_attachments(swapchain.m_num_images);
    for (size_t i = 0; i < swapchain.m_num_images; i++)
    {
        swapchain_color_attachments[i] = Gp::Texture(&device, swapchain, i);
    }

    std::vector<Gp::FramebufferBindings> framebuffer_bindings_ssao(swapchain.m_num_images);
    for (size_t i = 0; i < 3; i++)
    {
        framebuffer_bindings_ssao[i] =
            Gp::FramebufferBindings(&device, { &swapchain_color_attachments[i] });
    }

    // ssao pipeline
    Gp::RasterPipeline ssao_pipeline = [&]() {
        // create pipeline for ssao
        Gp::ShaderSrc vertexShaderSrc2(Gp::ShaderStageEnum::Vertex);
        vertexShaderSrc2.m_entry = "vs_main";
        vertexShaderSrc2.m_file_path =
            "../src/graphicsapi/example/00ambientocclusion/00ssao.hlsl";

        Gp::ShaderSrc fragmentShaderSrc2(Gp::ShaderStageEnum::Fragment);
        fragmentShaderSrc2.m_entry = "fs_main";
        fragmentShaderSrc2.m_file_path =
            "../src/graphicsapi/example/00ambientocclusion/00ssao.hlsl";
        std::vector<Gp::ShaderSrc> ssao_shader_srcs;
        ssao_shader_srcs.push_back(vertexShaderSrc2);
        ssao_shader_srcs.push_back(fragmentShaderSrc2);

        return Gp::RasterPipeline(&device, ssao_shader_srcs, framebuffer_bindings_ssao[0]);
    }();

    Gp::Texture rt_result(&device,
                          Gp::TextureUsageEnum::StorageImage |
                              Gp::TextureUsageEnum::ColorAttachment | Gp::TextureUsageEnum::Sampled,
                          Gp::TextureStateEnum::NonFragmentShaderVisible,
                          Gp::FormatEnum::R11G11B10_UFloat,
                          m_resolution,
                          nullptr,
                          nullptr,
                          float4(0.0f, 0.0f, 0.0f, 0.0f),
                          "ray_tracing_result");

    // raytracing pipeline
    Gp::RayTracingPipeline raytrace_pipeline = [&]() {
        // create pipeline for ssao
        Gp::ShaderSrc raygen_shader(Gp::ShaderStageEnum::RayGen);
        raygen_shader.m_entry = "RayGen";
        raygen_shader.m_file_path =
            "../src/graphicsapi/example/00ambientocclusion/RayGen.hlsl";

        Gp::ShaderSrc hit_shader(Gp::ShaderStageEnum::ClosestHit);
        hit_shader.m_entry = "ClosestHit";
        hit_shader.m_file_path =
            "../src/graphicsapi/example/00ambientocclusion/ClosestHit.hlsl";

        Gp::ShaderSrc hit_shader2(Gp::ShaderStageEnum::ClosestHit);
        hit_shader2.m_entry = "ClosestHit";
        hit_shader2.m_file_path =
            "../src/graphicsapi/example/00ambientocclusion/ClosestHit2.hlsl";

        Gp::ShaderSrc miss_shader(Gp::ShaderStageEnum::Miss);
        miss_shader.m_entry = "Miss";
        miss_shader.m_file_path =
            "../src/graphicsapi/example/00ambientocclusion/Miss.hlsl";

        Gp::RayTracingPipelineConfig rt_config;
        [[maybe_unused]] size_t      raygen_id = rt_config.add_shader(raygen_shader);
        [[maybe_unused]] size_t      miss_id   = rt_config.add_shader(miss_shader);

        [[maybe_unused]] size_t closesthit_id = rt_config.add_shader(hit_shader);
        [[maybe_unused]] size_t hitgroup_id   = rt_config.add_hit_group(closesthit_id);

        [[maybe_unused]] size_t closesthit2_id = rt_config.add_shader(hit_shader2);
        [[maybe_unused]] size_t hitgroup_id2   = rt_config.add_hit_group(closesthit2_id);

        return Gp::RayTracingPipeline(&device, rt_config, 32, 32, 1, "raytracing_pipeline");
    }();

    Gp::Sampler sampler(&device);

    Gp::StagingBufferManager temp_resource_manager(&device, "staging_buffer_manager");

    AssetManager asset_manager(&device, &temp_resource_manager);
    int2         pbr_mesh_range = asset_manager.add_pbr_object("sponza/sponza.obj");
    // int2 pbr_mesh_range = asset_manager.add_pbr_object("cube.obj");

    std::vector<Gp::RayTracingBlas>         blases;
    size_t                                  num_geometries = pbr_mesh_range.y - pbr_mesh_range.x;
    std::vector<Gp::RayTracingGeometryDesc> raytracing_geom_descs(num_geometries);

    for (size_t i = pbr_mesh_range.x; i < pbr_mesh_range.y; i++)
    {
        const PbrObject & object = asset_manager.m_pbr_objects[i];
        const Mesh &      mesh   = asset_manager.m_meshes[object.m_mesh_id];

        raytracing_geom_descs[i].set_flag(Gp::RayTracingGeometryFlag::Opaque);
        raytracing_geom_descs[i].set_index_buffer(mesh.m_index_buffer,
                                                  mesh.get_num_indices(object.m_submesh_id),
                                                  sizeof(uint32_t),
                                                  Gp::IndexType::Uint32,
                                                  mesh.m_offset_in_indices[object.m_submesh_id]);
        raytracing_geom_descs[i].set_vertex_buffer(mesh.m_vertex_buffer,
                                                   mesh.get_num_vertices(object.m_submesh_id),
                                                   sizeof(HlslVertexIn),
                                                   Gp::FormatEnum::R32G32B32_SFloat,
                                                   mesh.m_offset_in_vertices[object.m_submesh_id]);
    }
    blases.push_back(Gp::RayTracingBlas(&device,
                                        raytracing_geom_descs.data(),
                                        raytracing_geom_descs.size(),
                                        &temp_resource_manager,
                                        "ray_tracing_blas"));
    temp_resource_manager.submit_all_pending_upload();

    std::vector<Gp::RayTracingInstance> instances;
    for (size_t i = 0; i < blases.size(); i++)
    {
        instances.emplace_back(&blases[i], 0);
    }
    Gp::RayTracingTlas tlas(&device,
                            instances.data(),
                            instances.size(),
                            &temp_resource_manager,
                            "ray_tracing_tlas");

    Gp::Buffer gpu_ssao_params(&device,
                               Gp::BufferUsageEnum::ConstantBuffer,
                               Gp::MemoryUsageEnum::CpuToGpu,
                               sizeof(SsaoParams),
                               nullptr,
                               nullptr,
                               "ssao_param");

    // ssao params
    SsaoParams ssao_params;
    ssao_params.m_color = float4(0.0f, 0.0f, 0.1f, 0.0f);
    std::memcpy(gpu_ssao_params.map(), &ssao_params, sizeof(ssao_params));
    gpu_ssao_params.unmap();

    FpsCamera fps_camera(float3(10.0f, 10.0f, 10.0f),
                         float3(0, 0, 0),
                         float3(0, 1, 0),
                         radians(60.0f),
                         float(m_resolution.x) / float(m_resolution.y));

    struct CameraParams
    {
        float4x4 m_inv_view;
        float4x4 m_inv_proj;
    };

    Gp::Buffer cb_params(&device, Gp::BufferUsageEnum::ConstantBuffer, Gp::MemoryUsageEnum::CpuOnly, sizeof(CameraParams));

    // create material buffer
    Gp::Buffer material_buffer(&device,
                               Gp::BufferUsageEnum::StorageBuffer,
                               Gp::MemoryUsageEnum::GpuOnly,
                               sizeof(PbrMaterial) * 100,
                               reinterpret_cast<std::byte *>(asset_manager.m_pbr_materials.data()),
                               &temp_resource_manager,
                               "material_buffer");
    temp_resource_manager.submit_all_pending_upload();

    std::vector<uint32_t> material_ids(pbr_mesh_range.y - pbr_mesh_range.x);
    for (size_t i = pbr_mesh_range.x; i < pbr_mesh_range.y; i++)
    {
        const PbrObject & object = asset_manager.m_pbr_objects[i];
        material_ids[i]          = static_cast<int>(object.m_material_id);
    }
    Gp::Buffer material_id_buffer(&device,
                                  Gp::BufferUsageEnum::StorageBuffer,
                                  Gp::MemoryUsageEnum::GpuOnly,
                                  sizeof(uint32_t) * 100,
                                  reinterpret_cast<std::byte *>(material_ids.data()),
                                  &temp_resource_manager,
                                  "material_id_buffer");
    temp_resource_manager.submit_all_pending_upload();

    std::vector<Gp::CommandPool>    cmd_pools(num_flights);
    std::vector<Gp::DescriptorPool> desc_pools(num_flights);
    for (size_t i = 0; i < num_flights; i++)
    {
        desc_pools[i] = Gp::DescriptorPool(&device);
        cmd_pools[i]  = Gp::CommandPool(&device);
    }

    Gp::RayTracingShaderTable raytrace_shader_table(&device,
                                                    raytrace_pipeline,
                                                    "raytracing_shader_table");

    Gp::Semaphore semaphore(&device);
    Gp::Semaphore semaphore2(&device);

    size_t i_flight = 0;

    while (!m_window.should_close_window())
    {
        const float frame_time = 0.01f;
        fps_camera.update(&m_window, frame_time);
        auto transform = fps_camera.get_camera_props();

        CameraParams cam_params;
        cam_params.m_inv_view = inverse(transform.m_view);
        cam_params.m_inv_proj = inverse(transform.m_proj);
        std::memcpy(cb_params.map(), &cam_params, sizeof(cam_params));
        cb_params.unmap();

        swapchain.update_image_index(&semaphore);

        fences[i_flight].wait();
        fences[i_flight].reset();
        cmd_pools[i_flight].reset();
        desc_pools[i_flight].reset();

        Gp::CommandList cmd_list = cmd_pools[i_flight].get_command_list();

        // begin
        cmd_list.begin();

        // do raytracing
        cmd_list.bind_raytrace_pipeline(raytrace_pipeline);

        // set ray desc
        std::array<Gp::DescriptorSet, 3> ray_descriptor_sets;
        ray_descriptor_sets[0] =
            Gp::DescriptorSet(&device, raytrace_pipeline, &desc_pools[i_flight], 0, "rt_desc_set0");
        ray_descriptor_sets[1] =
            Gp::DescriptorSet(&device, raytrace_pipeline, &desc_pools[i_flight], 1, "rt_desc_set1");
        ray_descriptor_sets[2] =
            Gp::DescriptorSet(&device, raytrace_pipeline, &desc_pools[i_flight], 2, "rt_desc_set2");
        ray_descriptor_sets[0]
            .set_b_constant_buffer(0, cb_params)
            .set_u_rw_texture(0, rt_result)
            .set_t_ray_tracing_accel(0, tlas)
            .set_t_structured_buffer(1,
                                     material_buffer,
                                     sizeof(PbrMaterial),
                                     asset_manager.m_pbr_materials.size())
            .set_t_structured_buffer(2, material_id_buffer, sizeof(uint32_t), material_ids.size())
            .set_s_sampler(0, sampler);
        for (size_t i = pbr_mesh_range.x; i < pbr_mesh_range.y; i++)
        {
            const PbrObject & object = asset_manager.m_pbr_objects[i];
            const Mesh &      mesh   = asset_manager.m_meshes[object.m_mesh_id];
            ray_descriptor_sets[0].set_t_byte_address_buffer(3,
                                                             mesh.m_index_buffer,
                                                             sizeof(uint32_t),
                                                             mesh.get_num_indices(i),
                                                             i,
                                                             mesh.m_offset_in_indices[i]);
        }
        ray_descriptor_sets[0].update();
        for (size_t i = pbr_mesh_range.x; i < pbr_mesh_range.y; i++)
        {
            const PbrObject & object = asset_manager.m_pbr_objects[i];
            const Mesh &      mesh   = asset_manager.m_meshes[object.m_mesh_id];
            ray_descriptor_sets[1].set_t_structured_buffer(0,
                                                           mesh.m_vertex_buffer,
                                                           sizeof(HlslVertexIn),
                                                           mesh.get_num_vertices(i),
                                                           i,
                                                           mesh.m_offset_in_vertices[i]);
        }
        ray_descriptor_sets[1].update();
        for (size_t i = 0; i < asset_manager.m_textures.size(); i++)
        {
            ray_descriptor_sets[2].set_t_texture(0, asset_manager.m_textures[i], i);
        }
        ray_descriptor_sets[2].update();

        // ray tracing pass
        cmd_list.bind_raytrace_descriptor_set(ray_descriptor_sets);
        cmd_list.trace_rays(raytrace_shader_table, m_resolution.x, m_resolution.y);

        // transition
        cmd_list.transition_texture(rt_result,
                                    Gp::TextureStateEnum::NonFragmentShaderVisible,
                                    Gp::TextureStateEnum::FragmentShaderVisible);
        cmd_list.transition_texture(swapchain_color_attachments[swapchain.m_image_index],
                                    Gp::TextureStateEnum::Present,
                                    Gp::TextureStateEnum::ColorAttachment);

        // draw result
        cmd_list.begin_render_pass(framebuffer_bindings_ssao[swapchain.m_image_index]);
        cmd_list.bind_raster_pipeline(ssao_pipeline);

        // set ssao params
        std::array<Gp::DescriptorSet, 1> ssao_descriptor_sets;
        ssao_descriptor_sets[0] = Gp::DescriptorSet(&device, ssao_pipeline, &desc_pools[i_flight], 0);
        ssao_descriptor_sets[0].set_t_texture(0, rt_result).set_s_sampler(0, sampler).update();

        // raster
        cmd_list.bind_graphics_descriptor_set(ssao_descriptor_sets);
        cmd_list.bind_vertex_buffer(asset_manager.m_meshes[0].m_vertex_buffer, sizeof(HlslVertexIn));
        cmd_list.bind_index_buffer(asset_manager.m_meshes[0].m_index_buffer, Gp::IndexType::Uint32);
        cmd_list.draw_instanced(3, 1, 0, 0);
        cmd_list.end_render_pass();

        // transition
        cmd_list.transition_texture(swapchain_color_attachments[swapchain.m_image_index],
                                    Gp::TextureStateEnum::ColorAttachment,
                                    Gp::TextureStateEnum::Present);
        cmd_list.transition_texture(rt_result,
                                    Gp::TextureStateEnum::FragmentShaderVisible,
                                    Gp::TextureStateEnum::NonFragmentShaderVisible);

        // end and submit
        cmd_list.end();
        cmd_list.submit(&fences[i_flight], &semaphore, &semaphore2);

        swapchain.present(&semaphore2);
        m_window.update();
        GlfwHandler::Inst().poll_events();

        i_flight = (i_flight + 1) % num_flights;
    }

    for (int i = 0; i < swapchain.m_num_images; i++)
    {
        fences[i].wait();
    }

    return 0;
}