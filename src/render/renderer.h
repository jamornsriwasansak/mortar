#pragma once

#include "common/camera_params.h"
#include "common/render_params.h"
#include "rhi/rhi.h"
#include "passes/restir_direct_light/bindless_object_table.h"
#include "passes/restir_direct_light/direct_light_params.h"
#include "passes/restir_direct_light/reservior.h"
#include "passes/rtvisualize/rtvisualize.h"
#include "rendercontext.h"
#include "scene.h"

struct Renderer
{
    Rhi::RasterPipeline                   m_raster_pipeline;
    Rhi::RayTracingPipeline               m_rt_pipeline;
    Rhi::RayTracingShaderTable            m_rt_sbt;
    Rhi::RayTracingBlas                   m_rt_static_mesh_nonemissive_blas;
    Rhi::RayTracingBlas                   m_rt_static_mesh_emissive_blas;
    Rhi::RayTracingTlas                   m_rt_tlas;
    std::vector<Rhi::FramebufferBindings> m_raster_fbindings;
    std::array<Rhi::Texture, 2>           m_rt_results;
    std::array<Rhi::Buffer, 2>            m_prev_frame_reserviors;
    Rhi::Sampler                          m_sampler;

    using IndexBuffer  = SharedIndexBuffer;
    using VertexBuffer = SharedVertexBuffer;

    Rhi::Buffer m_cb_camera_params;
    Rhi::Buffer m_cb_directlight_params;
    Rhi::Buffer m_cb_materials;
    Rhi::Buffer m_cb_material_id;
    Rhi::Buffer m_cb_num_triangles;
    Rhi::Buffer m_cb_bindless_object_table;
    Rhi::Buffer m_cb_emissives;

    RtVisualizePass m_pass_rt_visualize;

    Renderer() {}

    void
    init(Rhi::Device * device, const int2 resolution, const std::vector<Rhi::Texture> & swapchain_attachment)
    {
        m_sampler = Rhi::Sampler(device);

        // create camera params buffer
        m_cb_camera_params =
            Rhi::Buffer(device, Rhi::BufferUsageEnum::ConstantBuffer, Rhi::MemoryUsageEnum::CpuToGpu, sizeof(CameraParams));
        m_cb_directlight_params = Rhi::Buffer(device,
                                             Rhi::BufferUsageEnum::ConstantBuffer,
                                             Rhi::MemoryUsageEnum::CpuToGpu,
                                             sizeof(DirectLightParams));

        init_or_resize_resolution(device, resolution, swapchain_attachment);
        init_shaders(device, true);

        m_pass_rt_visualize = RtVisualizePass(*device);
    }

    void
    init_or_resize_resolution(Rhi::Device * device, const int2 resolution, const std::vector<Rhi::Texture> & swapchain_attachment)
    {
        m_raster_fbindings.resize(swapchain_attachment.size());
        for (size_t i = 0; i < swapchain_attachment.size(); i++)
        {
            m_raster_fbindings[i] = Rhi::FramebufferBindings(device, { &swapchain_attachment[i] });
        }

        for (size_t i = 0; i < 2; i++)
        {
            m_rt_results[i]            = Rhi::Texture(device,
                                          Rhi::TextureUsageEnum::StorageImage | Rhi::TextureUsageEnum::ColorAttachment |
                                              Rhi::TextureUsageEnum::Sampled,
                                          Rhi::TextureStateEnum::NonFragmentShaderVisible,
                                          Rhi::FormatEnum::R32G32B32A32_SFloat,
                                          resolution,
                                          nullptr,
                                          nullptr,
                                          float4(0.0f, 0.0f, 0.0f, 0.0f),
                                          "ray_tracing_result");
            m_prev_frame_reserviors[i] = Rhi::Buffer(device,
                                                    Rhi::BufferUsageEnum::StorageBuffer,
                                                    Rhi::MemoryUsageEnum::GpuOnly,
                                                    sizeof(Reservior) * resolution.x * resolution.y,
                                                    nullptr,
                                                    nullptr,
                                                    "prev_frame_reservior");
        }
    }

    void
    init_shaders(Rhi::Device * device, const bool is_first_time)
    {
        // raster pipeline
        try
        {
            m_raster_pipeline = [&]() {
                std::filesystem::path shader_path = "../src/render/passes/beauty/";
                Rhi::ShaderSrc         vertexShaderSrc2(Rhi::ShaderStageEnum::Vertex);
                vertexShaderSrc2.m_entry     = "VsMain";
                vertexShaderSrc2.m_file_path = shader_path / "beauty.hlsl";

                Rhi::ShaderSrc fragmentShaderSrc2(Rhi::ShaderStageEnum::Fragment);
                fragmentShaderSrc2.m_entry     = "FsMain";
                fragmentShaderSrc2.m_file_path = shader_path / "beauty.hlsl";

                std::vector<Rhi::ShaderSrc> ssao_shader_srcs;
                ssao_shader_srcs.push_back(vertexShaderSrc2);
                ssao_shader_srcs.push_back(fragmentShaderSrc2);

                return Rhi::RasterPipeline(device, ssao_shader_srcs, m_raster_fbindings[0]);
            }();

            // raytracing pipeline
            m_rt_pipeline = [&]() {
                std::filesystem::path shader_path = "../src/render/passes/restir_direct_light/";

                // create pipeline for ssao
                Rhi::ShaderSrc raygen_shader(Rhi::ShaderStageEnum::RayGen);
                raygen_shader.m_entry     = "RayGen";
                raygen_shader.m_file_path = shader_path / "direct_light.h";

                Rhi::ShaderSrc standard_hit_shader(Rhi::ShaderStageEnum::ClosestHit);
                standard_hit_shader.m_entry     = "ClosestHit";
                standard_hit_shader.m_file_path = shader_path / "direct_light.h";

                Rhi::ShaderSrc emissive_hit_shader(Rhi::ShaderStageEnum::ClosestHit);
                emissive_hit_shader.m_entry     = "EmissiveClosestHit";
                emissive_hit_shader.m_file_path = shader_path / "direct_light.h";

                Rhi::ShaderSrc miss_shader(Rhi::ShaderStageEnum::Miss);
                miss_shader.m_entry     = "Miss";
                miss_shader.m_file_path = shader_path / "direct_light.h";

                Rhi::ShaderSrc shadow_miss_shader(Rhi::ShaderStageEnum::Miss);
                shadow_miss_shader.m_entry     = "ShadowMiss";
                shadow_miss_shader.m_file_path = shader_path / "direct_light.h";

                Rhi::RayTracingPipelineConfig rt_config;

                [[maybe_unused]] size_t raygen_id = rt_config.add_shader(raygen_shader);
                [[maybe_unused]] size_t miss_id   = rt_config.add_shader(miss_shader);
                [[maybe_unused]] size_t miss_id2  = rt_config.add_shader(miss_shader);

                size_t                  closesthit_id = rt_config.add_shader(standard_hit_shader);
                [[maybe_unused]] size_t hitgroup_id   = rt_config.add_hit_group(closesthit_id);

                size_t emissive_closesthit_id = rt_config.add_shader(emissive_hit_shader);
                [[maybe_unused]] size_t emisive_hitgroup_id = rt_config.add_hit_group(emissive_closesthit_id);

                return Rhi::RayTracingPipeline(device, rt_config, 16, 64, 2, "raytracing_pipeline");
            }();

            // raytracing visualize pipeline

            m_rt_sbt = Rhi::RayTracingShaderTable(device, m_rt_pipeline, "raytracing_shader_table");
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
        std::vector<Rhi::RayTracingGeometryDesc> static_nonemissive_mesh_descs;
        std::vector<Rhi::RayTracingGeometryDesc> static_emissive_mesh_descs;
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

                Rhi::RayTracingGeometryDesc desc;
                desc.set_flag(Rhi::RayTracingGeometryFlag::Opaque);
                desc.set_index_buffer(index_buffer.m_buffer,
                                      mesh.m_index_buffer_offset * Rhi::GetSizeInBytes(index_buffer.m_type),
                                      index_buffer.m_type,
                                      mesh.m_num_indices);
                desc.set_vertex_buffer(vertex_buffer.m_buffer,
                                       mesh.m_vertex_buffer_offset * sizeof(CompactVertex),
                                       Rhi::FormatEnum::R32G32B32_SFloat,
                                       sizeof(CompactVertex),
                                       mesh.m_num_vertices);
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

                Rhi::RayTracingGeometryDesc desc;
                desc.set_flag(Rhi::RayTracingGeometryFlag::Opaque);
                desc.set_index_buffer(index_buffer.m_buffer,
                                      mesh.m_index_buffer_offset * Rhi::GetSizeInBytes(index_buffer.m_type),
                                      index_buffer.m_type,
                                      mesh.m_num_indices);
                desc.set_vertex_buffer(vertex_buffer.m_buffer,
                                       mesh.m_vertex_buffer_offset * sizeof(CompactVertex),
                                       Rhi::FormatEnum::R32G32B32_SFloat,
                                       sizeof(CompactVertex),
                                       mesh.m_num_vertices);
                static_emissive_mesh_descs.emplace_back(desc);

                // setup num triangles
                num_triangles.push_back(mesh.m_num_indices / 3);
            }
        }

        m_rt_static_mesh_nonemissive_blas =
            Rhi::RayTracingBlas(ctx.m_device,
                               static_nonemissive_mesh_descs.data(),
                               static_nonemissive_mesh_descs.size(),
                               ctx.m_staging_buffer_manager,
                               "ray_tracing_staitc_nonemissive_blas");
        m_rt_static_mesh_emissive_blas = Rhi::RayTracingBlas(ctx.m_device,
                                                            static_emissive_mesh_descs.data(),
                                                            static_emissive_mesh_descs.size(),
                                                            ctx.m_staging_buffer_manager,
                                                            "ray_tracing_static_emissive_blas");
        ctx.m_staging_buffer_manager->submit_all_pending_upload();

        Rhi::RayTracingInstance non_emissive_instance(&m_rt_static_mesh_nonemissive_blas, 0);
        Rhi::RayTracingInstance emissive_instance(&m_rt_static_mesh_emissive_blas, 1);
        std::array<const Rhi::RayTracingInstance *, 2> instances{ &non_emissive_instance, &emissive_instance };

        m_rt_tlas = Rhi::RayTracingTlas(ctx.m_device,
                                       instances,
                                       ctx.m_staging_buffer_manager,
                                       "ray_tracing_tlas");

        // TODO:: the following buffers should be GPU only and updated iff the info is dirty
        // create material buffer
        m_cb_materials =
            Rhi::Buffer(ctx.m_device,
                       Rhi::BufferUsageEnum::ConstantBuffer,
                       Rhi::MemoryUsageEnum::GpuOnly,
                       sizeof(StandardMaterial) * 100,
                       reinterpret_cast<std::byte *>(params.m_asset_pool->m_standard_materials.data()),
                       ctx.m_staging_buffer_manager,
                       "material_buffer");
        ctx.m_staging_buffer_manager->submit_all_pending_upload();

        // create material id buffer
        m_cb_material_id = Rhi::Buffer(ctx.m_device,
                                      Rhi::BufferUsageEnum::ConstantBuffer,
                                      Rhi::MemoryUsageEnum::GpuOnly,
                                      sizeof(uint32_t) * 100,
                                      reinterpret_cast<std::byte *>(mat_ids.data()),
                                      ctx.m_staging_buffer_manager,
                                      "material_id");

        // create num triangles buffer
        m_cb_num_triangles = Rhi::Buffer(ctx.m_device,
                                        Rhi::BufferUsageEnum::ConstantBuffer,
                                        Rhi::MemoryUsageEnum::GpuOnly,
                                        sizeof(uint32_t) * 100,
                                        reinterpret_cast<std::byte *>(num_triangles.data()),
                                        ctx.m_staging_buffer_manager,
                                        "num_triangles_buffer");

        // bindless object table
        m_cb_bindless_object_table = Rhi::Buffer(ctx.m_device,
                                                Rhi::BufferUsageEnum::ConstantBuffer,
                                                Rhi::MemoryUsageEnum::CpuOnly,
                                                sizeof(BindlessObjectTable),
                                                reinterpret_cast<std::byte *>(num_triangles.data()),
                                                ctx.m_staging_buffer_manager,
                                                "object_table");

        // emissive info buffer
        m_cb_emissives =
            Rhi::Buffer(ctx.m_device,
                       Rhi::BufferUsageEnum::StorageBuffer,
                       Rhi::MemoryUsageEnum::GpuOnly,
                       sizeof(StandardEmissive) * 100,
                       reinterpret_cast<std::byte *>(params.m_asset_pool->m_standard_emissives.data()),
                       ctx.m_staging_buffer_manager,
                       "emissives");
        ctx.m_staging_buffer_manager->submit_all_pending_upload();

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
        Rhi::Device *    device = ctx.m_device;
        Rhi::CommandList cmds   = ctx.m_graphics_command_pool->get_command_list();

        if (params.m_is_static_mesh_dirty || notinit)
        {
            update_static_mesh(ctx, params);
        }

        if (params.m_is_shaders_dirty)
        {
            init_shaders(device, false);
            m_pass_rt_visualize.init_or_reload_shader(*device);
        }

        DirectLightParams dl_params;
        dl_params.m_rng_stream_id = frame_count++;
        std::memcpy(m_cb_directlight_params.map(), &dl_params, sizeof(DirectLightParams));
        m_cb_directlight_params.unmap();

        cmds.begin();

        // do raytracing
        cmds.bind_raytrace_pipeline(m_rt_pipeline);

        // set ray desc
        std::array<Rhi::DescriptorSet, 4> ray_descriptor_sets;
        ray_descriptor_sets[0] =
            Rhi::DescriptorSet(device, m_rt_pipeline, ctx.m_descriptor_pool, 0, "rt_desc_set0");
        ray_descriptor_sets[1] =
            Rhi::DescriptorSet(device, m_rt_pipeline, ctx.m_descriptor_pool, 1, "rt_desc_set1");
        ray_descriptor_sets[2] =
            Rhi::DescriptorSet(device, m_rt_pipeline, ctx.m_descriptor_pool, 2, "rt_desc_set2");
        ray_descriptor_sets[3] =
            Rhi::DescriptorSet(device, m_rt_pipeline, ctx.m_descriptor_pool, 3, "rt_desc_set3");

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
                .set_u_rw_structured_buffer(1,
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
                .set_b_constant_buffer(1, m_cb_material_id)
                .set_t_structured_buffer(0, m_cb_emissives, 100, sizeof(StandardEmissive));
            // set bindless textures
            for (size_t i = 0; i < 100; i++)
            {
                if (i < params.m_asset_pool->m_textures.size())
                {
                    ray_descriptor_sets[1].set_t_texture(1, params.m_asset_pool->m_textures[i], i);
                }
                else
                {
                    ray_descriptor_sets[1].set_t_texture(1, *ctx.m_dummy_texture, i);
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

        bool x = true;

        // ray tracing pass
        cmds.bind_raytrace_descriptor_set(ray_descriptor_sets);
        //cmds.trace_rays(m_rt_sbt, params.m_resolution.x, params.m_resolution.y);

        m_pass_rt_visualize.run(cmds, ctx, params, m_rt_tlas, m_rt_results[ctx.m_flight_index % 2], params.m_resolution);

        {
            // transition
            cmds.transition_texture(m_rt_results[ctx.m_flight_index % 2],
                                    Rhi::TextureStateEnum::NonFragmentShaderVisible,
                                    Rhi::TextureStateEnum::FragmentShaderVisible);
            cmds.transition_texture(*ctx.m_swapchain_texture,
                                    Rhi::TextureStateEnum::Present,
                                    Rhi::TextureStateEnum::ColorAttachment);

            // draw result
            cmds.begin_render_pass(m_raster_fbindings[ctx.m_image_index]);
            {
                cmds.bind_raster_pipeline(m_raster_pipeline);

                // set ssao params
                std::array<Rhi::DescriptorSet, 1> beauty_desc_sets;
                beauty_desc_sets[0] = Rhi::DescriptorSet(device, m_raster_pipeline, ctx.m_descriptor_pool, 0);
                beauty_desc_sets[0]
                    .set_t_texture(0, m_rt_results[ctx.m_flight_index % 2])
                    .set_s_sampler(0, m_sampler)
                    .update();

                // raster
                cmds.bind_graphics_descriptor_set(beauty_desc_sets);
                cmds.bind_vertex_buffer(params.m_asset_pool->m_vertex_buffers[0].m_buffer, sizeof(CompactVertex));
                cmds.bind_index_buffer(params.m_asset_pool->m_index_buffers[0].m_buffer, Rhi::IndexType::Uint32);
                cmds.draw_instanced(3, 1, 0, 0);
            }
            cmds.end_render_pass();

            // render imgui onto swapchain
            if (params.m_should_imgui_drawn)
            {
                cmds.render_imgui(*ctx.m_imgui_render_pass, ctx.m_image_index);
            }

            // transition
            cmds.transition_texture(*ctx.m_swapchain_texture,
                                    Rhi::TextureStateEnum::ColorAttachment,
                                    Rhi::TextureStateEnum::Present);
            cmds.transition_texture(m_rt_results[ctx.m_flight_index % 2],
                                    Rhi::TextureStateEnum::FragmentShaderVisible,
                                    Rhi::TextureStateEnum::NonFragmentShaderVisible);

            cmds.end();
            cmds.submit(ctx.m_flight_fence, ctx.m_image_ready_semaphore, ctx.m_image_presentable_semaphore);
        }
    }
};