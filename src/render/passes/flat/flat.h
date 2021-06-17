#pragma once

#include "../../shared/compactvertex.h"
#include "../../shared/pbrmaterial.h"
#include "assetmanager.h"

#include "00constantbuffer.h"
#include "00ssaoparam.h"
#include "common/camera.h"
#include "graphicsapi/dxa/dxa.h"
#include "graphicsapi/vka/vka.h"

int
mainnn()
{
    int2 m_resolution = int2(1920, 1080);

    std::filesystem::path shader_path = "../src/render/integrators/flat/";

    // create glfw state
    Window m_window = Window("Mortar 2.0", m_resolution);

    // create entry
    const bool                      is_debug = false;
    Gp::Entry                       entry(m_window, is_debug);
    std::vector<Gp::PhysicalDevice> devices = entry.get_graphics_devices();
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

    std::vector<Gp::CommandPool>    cmd_pools(num_flights);
    std::vector<Gp::DescriptorPool> desc_pools(num_flights);
    for (size_t i = 0; i < num_flights; i++)
    {
        desc_pools[i] = Gp::DescriptorPool(&device);
        cmd_pools[i]  = Gp::CommandPool(&device);
    }

    // ssao pipeline
    Gp::RasterPipeline ssao_pipeline = [&]() {
        // create pipeline for ssao
        Gp::ShaderSrc vertexShaderSrc2(Gp::ShaderStageEnum::Vertex);
        vertexShaderSrc2.m_entry     = "vs_main";
        vertexShaderSrc2.m_file_path = shader_path / "00ssao.hlsl";

        Gp::ShaderSrc fragmentShaderSrc2(Gp::ShaderStageEnum::Fragment);
        fragmentShaderSrc2.m_entry     = "fs_main";
        fragmentShaderSrc2.m_file_path = shader_path / "00ssao.hlsl";
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
        raygen_shader.m_entry     = "RayGen";
        raygen_shader.m_file_path = shader_path / "RayGen.hlsl";

        Gp::ShaderSrc hit_shader(Gp::ShaderStageEnum::ClosestHit);
        hit_shader.m_entry     = "ClosestHit";
        hit_shader.m_file_path = shader_path / "ClosestHit.hlsl";

        Gp::ShaderSrc hit_shader2(Gp::ShaderStageEnum::ClosestHit);
        hit_shader2.m_entry     = "ClosestHit";
        hit_shader2.m_file_path = shader_path / "ClosestHit2.hlsl";

        Gp::ShaderSrc miss_shader(Gp::ShaderStageEnum::Miss);
        miss_shader.m_entry     = "Miss";
        miss_shader.m_file_path = shader_path / "Miss.hlsl";

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
        const PbrMesh & object = asset_manager.m_pbr_objects[i];
        const MeshBlob &      mesh   = asset_manager.m_mesh_blobs[object.m_blob_id];

        raytracing_geom_descs[i].set_flag(Gp::RayTracingGeometryFlag::Opaque);
        raytracing_geom_descs[i].set_index_buffer(mesh.m_index_buffer,
                                                  mesh.get_num_indices(object.m_subblob_id),
                                                  sizeof(uint32_t),
                                                  Gp::IndexType::Uint32,
                                                  mesh.m_offset_in_indices[object.m_subblob_id]);
        raytracing_geom_descs[i].set_vertex_buffer(mesh.m_vertex_buffer,
                                                   mesh.get_num_vertices(object.m_subblob_id),
                                                   sizeof(CompactVertex),
                                                   Gp::FormatEnum::R32G32B32_SFloat,
                                                   mesh.m_offset_in_vertices[object.m_subblob_id]);
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
    SsaoParams ssao_params = {};
    ssao_params.m_color    = float4(0.0f, 0.0f, 0.1f, 0.0f);
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
        const PbrMesh & object = asset_manager.m_pbr_objects[i];
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
            const PbrMesh & object = asset_manager.m_pbr_objects[i];
            const MeshBlob &      mesh   = asset_manager.m_mesh_blobs[object.m_blob_id];
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
            const PbrMesh & object = asset_manager.m_pbr_objects[i];
            const MeshBlob &      mesh   = asset_manager.m_mesh_blobs[object.m_blob_id];
            ray_descriptor_sets[1].set_t_structured_buffer(0,
                                                           mesh.m_vertex_buffer,
                                                           sizeof(CompactVertex),
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
        cmd_list.bind_vertex_buffer(asset_manager.m_mesh_blobs[0].m_vertex_buffer, sizeof(CompactVertex));
        cmd_list.bind_index_buffer(asset_manager.m_mesh_blobs[0].m_index_buffer, Gp::IndexType::Uint32);
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

    for (int i = 0; i < num_flights; i++)
    {
        fences[i].wait();
    }

    return 0;
}