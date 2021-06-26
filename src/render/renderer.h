#pragma once

#include "graphicsapi/graphicsapi.h"
#include "passes/restir_directlight/bindlessobjectable.h"
#include "passes/restir_directlight/directlight_params.h"
#include "passes/restir_directlight/reservior.h"
#include "rendercontext.h"
#include "shared/cameraparam.h"

struct RenderParams
{
    int2                          m_resolution = { 0, 0 };
    AssetPool *                   m_asset_pool = nullptr;
    FpsCamera *                   m_fps_camera = nullptr;
    std::vector<StandardObject> * m_static_objects;
    bool                          m_is_static_mesh_dirty = true;
    bool                          m_is_shaders_dirty     = false;
};

struct Renderer
{
    Gp::RasterPipeline                   m_raster_pipeline;
    Gp::RayTracingPipeline               m_rt_pipeline;
    Gp::RayTracingShaderTable            m_rt_sbt;
    Gp::RayTracingBlas                   m_rt_static_mesh_nonemissive_blas;
    Gp::RayTracingBlas                   m_rt_static_mesh_emissive_blas;
    Gp::RayTracingTlas                   m_rt_tlas;
    std::vector<Gp::FramebufferBindings> m_raster_fbindings;
    std::array<Gp::Texture, 2>           m_rt_results;
    std::array<Gp::Buffer, 2>            m_prev_frame_reserviors;
    Gp::Sampler                          m_sampler;

    Gp::Buffer m_cb_camera_params;
    Gp::Buffer m_cb_directlight_params;
    Gp::Buffer m_cb_materials;
    Gp::Buffer m_cb_material_id;
    Gp::Buffer m_cb_num_triangles;
    Gp::Buffer m_cb_bindless_object_table;
    Gp::Buffer m_cb_emissive_object_ids;

    Renderer() {}

    void
    init(Gp::Device * device, const int2 resolution, const std::vector<Gp::Texture> & swapchain_attachment)
    {
        m_sampler = Gp::Sampler(device);

        // create camera params buffer
        m_cb_camera_params =
            Gp::Buffer(device, Gp::BufferUsageEnum::ConstantBuffer, Gp::MemoryUsageEnum::CpuToGpu, sizeof(CameraParams));
        m_cb_directlight_params = Gp::Buffer(device,
                                             Gp::BufferUsageEnum::ConstantBuffer,
                                             Gp::MemoryUsageEnum::CpuToGpu,
                                             sizeof(DirectLightParams));

        resize(device, resolution, swapchain_attachment);
        init_shaders(device, true);
    }

    void
    resize(Gp::Device * device, const int2 resolution, const std::vector<Gp::Texture> & swapchain_attachment)
    {
        m_raster_fbindings.resize(swapchain_attachment.size());
        for (size_t i = 0; i < swapchain_attachment.size(); i++)
        {
            m_raster_fbindings[i] = Gp::FramebufferBindings(device, { &swapchain_attachment[i] });
        }

        for (size_t i = 0; i < 2; i++)
        {
            m_rt_results[i]            = Gp::Texture(device,
                                          Gp::TextureUsageEnum::StorageImage | Gp::TextureUsageEnum::ColorAttachment |
                                              Gp::TextureUsageEnum::Sampled,
                                          Gp::TextureStateEnum::NonFragmentShaderVisible,
                                          Gp::FormatEnum::R32G32B32A32_SFloat,
                                          resolution,
                                          nullptr,
                                          nullptr,
                                          float4(0.0f, 0.0f, 0.0f, 0.0f),
                                          "ray_tracing_result");
            m_prev_frame_reserviors[i] = Gp::Buffer(device,
                                                    Gp::BufferUsageEnum::StorageBuffer,
                                                    Gp::MemoryUsageEnum::GpuOnly,
                                                    sizeof(Reservior) * resolution.x * resolution.y,
                                                    nullptr,
                                                    nullptr,
                                                    "prev_frame_reservior");
        }
    }

    void
    init_shaders(Gp::Device * device, const bool is_first_time)
    {
        // raster pipeline
        try
        {
            m_raster_pipeline = [&]() {
                std::filesystem::path shader_path = "../src/render/passes/flat/";
                Gp::ShaderSrc         vertexShaderSrc2(Gp::ShaderStageEnum::Vertex);
                vertexShaderSrc2.m_entry     = "vs_main";
                vertexShaderSrc2.m_file_path = shader_path / "00ssao.hlsl";

                Gp::ShaderSrc fragmentShaderSrc2(Gp::ShaderStageEnum::Fragment);
                fragmentShaderSrc2.m_entry     = "fs_main";
                fragmentShaderSrc2.m_file_path = shader_path / "00ssao.hlsl";
                std::vector<Gp::ShaderSrc> ssao_shader_srcs;
                ssao_shader_srcs.push_back(vertexShaderSrc2);
                ssao_shader_srcs.push_back(fragmentShaderSrc2);

                return Gp::RasterPipeline(device, ssao_shader_srcs, m_raster_fbindings[0]);
            }();

            // raytracing pipeline
            m_rt_pipeline = [&]() {
                std::filesystem::path shader_path = "../src/render/passes/restir_directlight/";

                // create pipeline for ssao
                Gp::ShaderSrc raygen_shader(Gp::ShaderStageEnum::RayGen);
                raygen_shader.m_entry     = "RayGen";
                raygen_shader.m_file_path = shader_path / "directlight.hlsl";

                Gp::ShaderSrc standard_hit_shader(Gp::ShaderStageEnum::ClosestHit);
                standard_hit_shader.m_entry     = "ClosestHit";
                standard_hit_shader.m_file_path = shader_path / "directlight.hlsl";

                Gp::ShaderSrc emissive_hit_shader(Gp::ShaderStageEnum::ClosestHit);
                emissive_hit_shader.m_entry     = "EmissiveClosestHit";
                emissive_hit_shader.m_file_path = shader_path / "directlight.hlsl";

                /*
                Gp::ShaderSrc hit_shader2(Gp::ShaderStageEnum::ClosestHit);
                hit_shader2.m_entry     = "ClosestHit";
                hit_shader2.m_file_path = shader_path / "directlight.hlsl";
                */

                Gp::ShaderSrc miss_shader(Gp::ShaderStageEnum::Miss);
                miss_shader.m_entry     = "Miss";
                miss_shader.m_file_path = shader_path / "directlight.hlsl";

                Gp::ShaderSrc shadow_miss_shader(Gp::ShaderStageEnum::Miss);
                shadow_miss_shader.m_entry     = "ShadowMiss";
                shadow_miss_shader.m_file_path = shader_path / "directlight.hlsl";

                Gp::RayTracingPipelineConfig rt_config;

                [[maybe_unused]] size_t raygen_id = rt_config.add_shader(raygen_shader);
                [[maybe_unused]] size_t miss_id   = rt_config.add_shader(miss_shader);
                [[maybe_unused]] size_t miss_id2  = rt_config.add_shader(miss_shader);

                size_t                  closesthit_id = rt_config.add_shader(standard_hit_shader);
                [[maybe_unused]] size_t hitgroup_id   = rt_config.add_hit_group(closesthit_id);

                size_t emissive_closesthit_id = rt_config.add_shader(emissive_hit_shader);
                [[maybe_unused]] size_t emisive_hitgroup_id = rt_config.add_hit_group(emissive_closesthit_id);

                return Gp::RayTracingPipeline(device, rt_config, 16, 64, 2, "raytracing_pipeline");
            }();
            m_rt_sbt = Gp::RayTracingShaderTable(device, m_rt_pipeline, "raytracing_shader_table");
        }
        catch (const std::exception & e)
        {
            if (is_first_time)
            {
                throw e;
            }
            else
            {
                Logger::Info(__FUNCTION__, " cannot reload shader due to ", e.what());
            }
        }
    }

    // TODO:: this is a hack. remove this
    bool notinit = true;

    void
    update_static_mesh(const RenderContext & ctx, const RenderParams & params)
    {
        std::vector<Gp::RayTracingGeometryDesc> static_nonemissive_mesh_descs;
        std::vector<Gp::RayTracingGeometryDesc> static_emissive_mesh_descs;
        std::vector<uint32_t>                   num_triangles;
        std::vector<uint32_t>                   mat_ids;

        for (size_t i = 0; i < params.m_static_objects->size(); i++)
        {
            const StandardObject & object = params.m_static_objects->at(i);
            const StandardMesh &   mesh = params.m_asset_pool->m_standard_meshes[object.m_mesh_id];

            if (object.m_material_id != -1)
            {
                assert(object.m_emissive_id == -1);
                // set up raytracing geom descs
                const IndexBuffer & index_buffer = params.m_asset_pool->m_index_buffers[mesh.m_index_buffer_id];
                const VertexBuffer & vertex_buffer =
                    params.m_asset_pool->m_vertex_buffers[mesh.m_index_buffer_id];

                Gp::RayTracingGeometryDesc desc;
                desc.set_flag(Gp::RayTracingGeometryFlag::Opaque);
                desc.set_index_buffer(index_buffer.m_buffer,
                                      mesh.m_num_indices,
                                      sizeof(uint32_t),
                                      index_buffer.m_type,
                                      mesh.m_index_buffer_offset);
                desc.set_vertex_buffer(vertex_buffer.m_buffer,
                                       mesh.m_num_vertices,
                                       sizeof(CompactVertex),
                                       Gp::FormatEnum::R32G32B32_SFloat,
                                       mesh.m_vertex_buffer_offset);
                static_nonemissive_mesh_descs.emplace_back(desc);

                // setup material id
                mat_ids.push_back(object.m_material_id);
            }

            if (object.m_emissive_id != -1)
            {
                assert(object.m_material_id == -1);
                // set up raytracing geom descs
                const IndexBuffer & index_buffer = params.m_asset_pool->m_index_buffers[mesh.m_index_buffer_id];
                const VertexBuffer & vertex_buffer =
                    params.m_asset_pool->m_vertex_buffers[mesh.m_index_buffer_id];

                Gp::RayTracingGeometryDesc desc;
                desc.set_flag(Gp::RayTracingGeometryFlag::Opaque);
                desc.set_index_buffer(index_buffer.m_buffer,
                                      mesh.m_num_indices,
                                      sizeof(uint32_t),
                                      index_buffer.m_type,
                                      mesh.m_index_buffer_offset);
                desc.set_vertex_buffer(vertex_buffer.m_buffer,
                                       mesh.m_num_vertices,
                                       sizeof(CompactVertex),
                                       Gp::FormatEnum::R32G32B32_SFloat,
                                       mesh.m_vertex_buffer_offset);
                static_emissive_mesh_descs.emplace_back(desc);

                // setup num triangles
                num_triangles.push_back(mesh.m_num_indices / 3);
            }
        }

        m_rt_static_mesh_nonemissive_blas =
            Gp::RayTracingBlas(ctx.m_device,
                               static_nonemissive_mesh_descs.data(),
                               static_nonemissive_mesh_descs.size(),
                               ctx.m_staging_buffer_manager,
                               "ray_tracing_staitc_nonemissive_blas");
        m_rt_static_mesh_emissive_blas = Gp::RayTracingBlas(ctx.m_device,
                                                            static_emissive_mesh_descs.data(),
                                                            static_emissive_mesh_descs.size(),
                                                            ctx.m_staging_buffer_manager,
                                                            "ray_tracing_static_emissive_blas");
        ctx.m_staging_buffer_manager->submit_all_pending_upload();

        Gp::RayTracingInstance non_emissive_instance(&m_rt_static_mesh_nonemissive_blas, 0);
        Gp::RayTracingInstance emissive_instance(&m_rt_static_mesh_emissive_blas, 1);
        std::array<Gp::RayTracingInstance *, 2> instances{ &non_emissive_instance, &emissive_instance };

        m_rt_tlas = Gp::RayTracingTlas(ctx.m_device,
                                       instances,
                                       ctx.m_staging_buffer_manager,
                                       "ray_tracing_tlas");
        ctx.m_staging_buffer_manager->submit_all_pending_upload();

        // TODO:: the following buffers should be GPU only and updated iff the info is dirty
        // create material buffer
        m_cb_materials =
            Gp::Buffer(ctx.m_device,
                       Gp::BufferUsageEnum::ConstantBuffer,
                       Gp::MemoryUsageEnum::GpuOnly,
                       sizeof(StandardMaterial) * 100,
                       reinterpret_cast<std::byte *>(params.m_asset_pool->m_standard_materials.data()),
                       ctx.m_staging_buffer_manager,
                       "material_buffer");

        // create material id buffer
        m_cb_material_id = Gp::Buffer(ctx.m_device,
                                      Gp::BufferUsageEnum::ConstantBuffer,
                                      Gp::MemoryUsageEnum::GpuOnly,
                                      sizeof(uint32_t) * 100,
                                      reinterpret_cast<std::byte *>(mat_ids.data()),
                                      ctx.m_staging_buffer_manager,
                                      "material_id");

        // create num triangles buffer
        m_cb_num_triangles = Gp::Buffer(ctx.m_device,
                                        Gp::BufferUsageEnum::ConstantBuffer,
                                        Gp::MemoryUsageEnum::GpuOnly,
                                        sizeof(uint32_t) * 100,
                                        reinterpret_cast<std::byte *>(num_triangles.data()),
                                        ctx.m_staging_buffer_manager,
                                        "num_triangles");

        // bindless object table
        m_cb_bindless_object_table = Gp::Buffer(ctx.m_device,
                                                Gp::BufferUsageEnum::ConstantBuffer,
                                                Gp::MemoryUsageEnum::CpuOnly,
                                                sizeof(BindlessObjectTable),
                                                reinterpret_cast<std::byte *>(num_triangles.data()),
                                                ctx.m_staging_buffer_manager,
                                                "num_triangles");
        BindlessObjectTable bot;
        bot.m_nonemissive_object_start_index = 0;
        bot.m_num_nonemissive_objects        = static_nonemissive_mesh_descs.size();
        bot.m_emissive_object_start_index    = static_nonemissive_mesh_descs.size();
        bot.m_num_emissive_objects           = static_emissive_mesh_descs.size();
        std::memcpy(m_cb_bindless_object_table.map(), &bot, sizeof(bot));
        m_cb_bindless_object_table.unmap();

        ctx.m_staging_buffer_manager->submit_all_pending_upload();

        notinit = false;
    }

    int frame_count = 0;
    void
    loop(const RenderContext & ctx, const RenderParams & params)
    {
        Gp::Device *    device = ctx.m_device;
        Gp::CommandList cmds   = ctx.m_graphics_command_pool->get_command_list();

        if (params.m_is_static_mesh_dirty || notinit)
        {
            update_static_mesh(ctx, params);
        }

        if (params.m_is_shaders_dirty)
        {
            init_shaders(device, false);
        }

        DirectLightParams dl_params;
        dl_params.m_rng_offset = static_cast<float>(frame_count++);
        std::memcpy(m_cb_directlight_params.map(), &dl_params, sizeof(DirectLightParams));
        m_cb_directlight_params.unmap();

        cmds.begin();

        // do raytracing
        cmds.bind_raytrace_pipeline(m_rt_pipeline);

        // set ray desc
        std::array<Gp::DescriptorSet, 4> ray_descriptor_sets;
        ray_descriptor_sets[0] =
            Gp::DescriptorSet(device, m_rt_pipeline, ctx.m_descriptor_pool, 0, "rt_desc_set0");
        ray_descriptor_sets[1] =
            Gp::DescriptorSet(device, m_rt_pipeline, ctx.m_descriptor_pool, 1, "rt_desc_set1");
        ray_descriptor_sets[2] =
            Gp::DescriptorSet(device, m_rt_pipeline, ctx.m_descriptor_pool, 2, "rt_desc_set2");
        ray_descriptor_sets[3] =
            Gp::DescriptorSet(device, m_rt_pipeline, ctx.m_descriptor_pool, 3, "rt_desc_set3");

        // set camera uniform
        auto         transform = params.m_fps_camera->get_camera_props();
        CameraParams cam_params;
        cam_params.m_inv_view = inverse(transform.m_view);
        cam_params.m_inv_proj = inverse(transform.m_proj);
        std::memcpy(m_cb_camera_params.map(), &cam_params, sizeof(CameraParams));
        m_cb_camera_params.unmap();

        {
            ray_descriptor_sets[0]
                .set_b_constant_buffer(0, m_cb_camera_params)
                .set_b_constant_buffer(1, m_cb_directlight_params)
                .set_t_structured_buffer(0,
                                         m_prev_frame_reserviors[(ctx.m_flight_index + 1) % 2],
                                         sizeof(Reservior),
                                         params.m_resolution.x * params.m_resolution.y,
                                         0,
                                         0)
                .set_u_rw_texture(0, m_rt_results[ctx.m_flight_index % 2])
                .set_u_rw_structure_buffer(1,
                                           m_prev_frame_reserviors[ctx.m_flight_index % 2],
                                           sizeof(Reservior),
                                           params.m_resolution.x * params.m_resolution.y,
                                           0,
                                           0)
                .update();
        }

        {
            ray_descriptor_sets[1]
                .set_s_sampler(0, m_sampler)
                .set_b_constant_buffer(0, m_cb_materials)
                .set_b_constant_buffer(1, m_cb_material_id);
            // set bindless textures
            for (size_t i = 0; i < 100; i++)
            {
                if (i < params.m_asset_pool->m_textures.size())
                {
                    ray_descriptor_sets[1].set_t_texture(0, params.m_asset_pool->m_textures[i], i);
                }
                else
                {
                    ray_descriptor_sets[1].set_t_texture(0, *ctx.m_dummy_texture, i);
                }
            }
            ray_descriptor_sets[1].update();
        }

        {
            ray_descriptor_sets[2]
                .set_t_ray_tracing_accel(0, m_rt_tlas)
                .set_b_constant_buffer(0, m_cb_bindless_object_table)
                .set_b_constant_buffer(1, m_cb_num_triangles);
            // set index buffer
            for (size_t i = 0; i < 100; i++)
            {
                if (i < params.m_static_objects->size())
                {
                    const StandardObject & object = params.m_static_objects->at(i);
                    const StandardMesh & mesh = params.m_asset_pool->m_standard_meshes[object.m_mesh_id];

                    const IndexBuffer & index_buffer =
                        params.m_asset_pool->m_index_buffers[mesh.m_index_buffer_id];
                    ray_descriptor_sets[2].set_t_byte_address_buffer(1,
                                                                     index_buffer.m_buffer,
                                                                     sizeof(uint32_t),
                                                                     mesh.m_num_indices,
                                                                     i,
                                                                     mesh.m_index_buffer_offset);
                }
                else
                {
                    ray_descriptor_sets[2].set_t_byte_address_buffer(1, *ctx.m_dummy_buffer, sizeof(uint32_t), 1, i, 0);
                }
            }
            ray_descriptor_sets[2].update();
        }

        {
            // set vertex buffers
            for (size_t i = 0; i < 100; i++)
            {
                if (i < params.m_static_objects->size())
                {
                    const StandardObject & object = params.m_static_objects->at(i);
                    const StandardMesh & mesh = params.m_asset_pool->m_standard_meshes[object.m_mesh_id];

                    const VertexBuffer & vertex_buffer =
                        params.m_asset_pool->m_vertex_buffers[mesh.m_index_buffer_id];
                    ray_descriptor_sets[3].set_t_structured_buffer(0,
                                                                   vertex_buffer.m_buffer,
                                                                   sizeof(CompactVertex),
                                                                   mesh.m_num_vertices,
                                                                   i,
                                                                   mesh.m_vertex_buffer_offset);
                }
                else
                {
                    ray_descriptor_sets[3].set_t_structured_buffer(0, *ctx.m_dummy_buffer, sizeof(uint32_t), 1, i, 0);
                }
            }
            ray_descriptor_sets[3].update();
        }

        // ray tracing pass
        cmds.bind_raytrace_descriptor_set(ray_descriptor_sets);
        cmds.trace_rays(m_rt_sbt, params.m_resolution.x, params.m_resolution.y);
        {
            // transition
            cmds.transition_texture(m_rt_results[ctx.m_flight_index % 2],
                                    Gp::TextureStateEnum::NonFragmentShaderVisible,
                                    Gp::TextureStateEnum::FragmentShaderVisible);
            cmds.transition_texture(*ctx.m_swapchain_texture,
                                    Gp::TextureStateEnum::Present,
                                    Gp::TextureStateEnum::ColorAttachment);

            // draw result
            cmds.begin_render_pass(m_raster_fbindings[ctx.m_image_index]);
            cmds.bind_raster_pipeline(m_raster_pipeline);

            // set ssao params
            std::array<Gp::DescriptorSet, 1> ssao_descriptor_sets;
            ssao_descriptor_sets[0] = Gp::DescriptorSet(device, m_raster_pipeline, ctx.m_descriptor_pool, 0);
            ssao_descriptor_sets[0]
                .set_t_texture(0, m_rt_results[ctx.m_flight_index % 2])
                .set_s_sampler(0, m_sampler)
                .update();

            // raster
            cmds.bind_graphics_descriptor_set(ssao_descriptor_sets);
            cmds.bind_vertex_buffer(params.m_asset_pool->m_vertex_buffers[0].m_buffer, sizeof(CompactVertex));
            cmds.bind_index_buffer(params.m_asset_pool->m_index_buffers[0].m_buffer, Gp::IndexType::Uint32);
            cmds.draw_instanced(3, 1, 0, 0);
            cmds.end_render_pass();

            // transition
            cmds.transition_texture(*ctx.m_swapchain_texture,
                                    Gp::TextureStateEnum::ColorAttachment,
                                    Gp::TextureStateEnum::Present);
            cmds.transition_texture(m_rt_results[ctx.m_flight_index % 2],
                                    Gp::TextureStateEnum::FragmentShaderVisible,
                                    Gp::TextureStateEnum::NonFragmentShaderVisible);

            cmds.end();
            cmds.submit(ctx.m_flight_fence, ctx.m_image_ready_semaphore, ctx.m_image_presentable_semaphore);
        }
    }
};