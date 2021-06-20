#pragma once

#include "graphicsapi/graphicsapi.h"
#include "rendercontext.h"
#include "shared/cameraparam.h"

struct RenderParams
{
    int2                        m_resolution     = { 0, 0 };
    std::vector<Gp::Texture> *  m_textures       = nullptr;
    std::vector<VertexBuffer> * m_vertex_buffers = nullptr;
    std::vector<IndexBuffer> *  m_index_buffers  = nullptr;
    std::vector<PbrMaterial> *  m_materials      = nullptr;
    FpsCamera *                 m_fps_camera     = nullptr;
    std::vector<PbrMesh>        m_static_meshes;
    bool                        m_is_static_mesh_dirty = true;
    bool                        m_is_shaders_dirty     = false;
};

struct Renderer
{
    Gp::RasterPipeline                   m_raster_pipeline;
    Gp::RayTracingPipeline               m_rt_pipeline;
    Gp::RayTracingShaderTable            m_rt_sbt;
    Gp::RayTracingBlas                   m_rt_static_mesh_blas;
    Gp::RayTracingTlas                   m_rt_tlas;
    std::vector<Gp::FramebufferBindings> m_raster_fbindings;
    Gp::Texture                          m_rt_result;
    Gp::Sampler                          m_sampler;

    Gp::Buffer m_cb_camera_params;
    Gp::Buffer m_cb_materials;
    Gp::Buffer m_cb_material_id;
    Gp::Buffer m_cb_num_triangles;

    Renderer() {}

    void
    init(Gp::Device * device, const int2 resolution, const std::vector<Gp::Texture> & swapchain_attachment)
    {
        m_sampler = Gp::Sampler(device);

        // create camera params buffer
        m_cb_camera_params =
            Gp::Buffer(device, Gp::BufferUsageEnum::ConstantBuffer, Gp::MemoryUsageEnum::CpuToGpu, sizeof(CameraParams));

        // TODO:: the following buffers should be GPU only and updated iff the info is dirty
        // is dirty create material buffer
        m_cb_materials = Gp::Buffer(device,
                                    Gp::BufferUsageEnum::ConstantBuffer,
                                    Gp::MemoryUsageEnum::CpuToGpu,
                                    sizeof(PbrMaterial) * 100,
                                    nullptr,
                                    nullptr,
                                    "material_buffer");

        // create material id buffer
        m_cb_material_id = Gp::Buffer(device,
                                      Gp::BufferUsageEnum::ConstantBuffer,
                                      Gp::MemoryUsageEnum::CpuToGpu,
                                      sizeof(uint32_t) * 100,
                                      nullptr,
                                      nullptr,
                                      "material_id");

        // create num triangles buffer
        m_cb_num_triangles = Gp::Buffer(device,
                                        Gp::BufferUsageEnum::ConstantBuffer,
                                        Gp::MemoryUsageEnum::CpuToGpu,
                                        sizeof(uint32_t) * 100,
                                        nullptr,
                                        nullptr,
                                        "num_triangles");

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

        m_rt_result = Gp::Texture(device,
                                  Gp::TextureUsageEnum::StorageImage |
                                      Gp::TextureUsageEnum::ColorAttachment | Gp::TextureUsageEnum::Sampled,
                                  Gp::TextureStateEnum::NonFragmentShaderVisible,
                                  Gp::FormatEnum::R11G11B10_UFloat,
                                  resolution,
                                  nullptr,
                                  nullptr,
                                  float4(0.0f, 0.0f, 0.0f, 0.0f),
                                  "ray_tracing_result");
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
                std::filesystem::path shader_path = "../src/render/passes/pathtracer/";

                // create pipeline for ssao
                Gp::ShaderSrc raygen_shader(Gp::ShaderStageEnum::RayGen);
                raygen_shader.m_entry     = "RayGen";
                raygen_shader.m_file_path = shader_path / "pathtracer.hlsl";

                Gp::ShaderSrc hit_shader(Gp::ShaderStageEnum::ClosestHit);
                hit_shader.m_entry     = "ClosestHit";
                hit_shader.m_file_path = shader_path / "pathtracer.hlsl";

                /*
                Gp::ShaderSrc hit_shader2(Gp::ShaderStageEnum::ClosestHit);
                hit_shader2.m_entry     = "ClosestHit";
                hit_shader2.m_file_path = shader_path / "pathtracer.hlsl";
                */

                Gp::ShaderSrc miss_shader(Gp::ShaderStageEnum::Miss);
                miss_shader.m_entry     = "Miss";
                miss_shader.m_file_path = shader_path / "pathtracer.hlsl";

                Gp::ShaderSrc shadow_miss_shader(Gp::ShaderStageEnum::Miss);
                shadow_miss_shader.m_entry     = "ShadowMiss";
                shadow_miss_shader.m_file_path = shader_path / "pathtracer.hlsl";


                Gp::RayTracingPipelineConfig rt_config;

                [[maybe_unused]] size_t raygen_id = rt_config.add_shader(raygen_shader);
                [[maybe_unused]] size_t miss_id   = rt_config.add_shader(miss_shader);
                [[maybe_unused]] size_t miss_id2  = rt_config.add_shader(miss_shader);

                [[maybe_unused]] size_t closesthit_id = rt_config.add_shader(hit_shader);
                [[maybe_unused]] size_t hitgroup_id   = rt_config.add_hit_group(closesthit_id);

                /*
                [[maybe_unused]] size_t closesthit2_id = rt_config.add_shader(hit_shader2);
                [[maybe_unused]] size_t hitgroup_id2   = rt_config.add_hit_group(closesthit2_id);
                */

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
    loop(const RenderContext & ctx, const RenderParams & params)
    {
        Gp::Device *    device = ctx.m_device;
        Gp::CommandList cmds   = ctx.m_graphics_command_pool->get_command_list();

        if (params.m_is_static_mesh_dirty || notinit)
        {
            size_t                                  num_geometries = params.m_static_meshes.size();
            std::vector<Gp::RayTracingGeometryDesc> raytracing_geom_descs(num_geometries);

            for (size_t i = 0; i < params.m_static_meshes.size(); i++)
            {
                const PbrMesh &     object       = params.m_static_meshes[i];
                const IndexBuffer & index_buffer = params.m_index_buffers->at(object.m_index_buffer_id);
                const VertexBuffer & vertex_buffer = params.m_vertex_buffers->at(object.m_index_buffer_id);
                raytracing_geom_descs[i].set_flag(Gp::RayTracingGeometryFlag::Opaque);
                raytracing_geom_descs[i].set_index_buffer(index_buffer.m_index_buffer,
                                                          object.m_num_indices,
                                                          sizeof(uint32_t),
                                                          index_buffer.m_type,
                                                          object.m_index_buffer_offset);
                raytracing_geom_descs[i].set_vertex_buffer(vertex_buffer.m_vertex_buffer,
                                                           object.m_num_vertices,
                                                           sizeof(CompactVertex),
                                                           Gp::FormatEnum::R32G32B32_SFloat,
                                                           object.m_vertex_buffer_offset);
            }
            m_rt_static_mesh_blas = Gp::RayTracingBlas(device,
                                                       raytracing_geom_descs.data(),
                                                       raytracing_geom_descs.size(),
                                                       ctx.m_staging_buffer_manager,
                                                       "ray_tracing_blas");
            ctx.m_staging_buffer_manager->submit_all_pending_upload();

            Gp::RayTracingInstance instance(&m_rt_static_mesh_blas, 0);

            m_rt_tlas = Gp::RayTracingTlas(device,
                                           &instance,
                                           1,
                                           ctx.m_staging_buffer_manager,
                                           "ray_tracing_tlas");
            ctx.m_staging_buffer_manager->submit_all_pending_upload();


            notinit = false;
        }

        if (params.m_is_shaders_dirty)
        {
            init_shaders(device, false);
        }

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

        // set material info uniform
        std::memcpy(m_cb_materials.map(),
                    params.m_materials->data(),
                    sizeof(PbrMaterial) * params.m_materials->size());
        m_cb_materials.unmap();

        // set material
        std::vector<uint32_t> mat_ids;
        for (size_t i = 0; i < params.m_static_meshes.size(); i++)
        {
            const PbrMesh & object = params.m_static_meshes[i];
            mat_ids.push_back(object.m_material_id);
        }
        std::memcpy(m_cb_material_id.map(), mat_ids.data(), sizeof(uint32_t) * mat_ids.size());
        m_cb_material_id.unmap();

        // set num triangles per mesh
        std::vector<uint32_t> num_triangles;
        for (size_t i = 0; i < params.m_static_meshes.size(); i++)
        {
            const PbrMesh & object = params.m_static_meshes[i];
            num_triangles.push_back(object.m_num_indices / 3);
        }
        std::memcpy(m_cb_num_triangles.map(), num_triangles.data(), sizeof(uint32_t) * num_triangles.size());
        m_cb_num_triangles.unmap();

        {
            ray_descriptor_sets[0]
                .set_b_constant_buffer(0, m_cb_camera_params)
                .set_u_rw_texture(0, m_rt_result)
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
                if (i < params.m_textures->size())
                {
                    ray_descriptor_sets[1].set_t_texture(0, params.m_textures->at(i), i);
                }
                else
                {
                    ray_descriptor_sets[1].set_t_texture(0, *ctx.m_dummy_texture, i);
                }
            }
            ray_descriptor_sets[1].update();
        }

        {
            ray_descriptor_sets[2].set_t_ray_tracing_accel(0, m_rt_tlas).set_b_constant_buffer(0, m_cb_num_triangles);
            // set index buffer
            for (size_t i = 0; i < 100; i++)
            {
                if (i < params.m_static_meshes.size())
                {
                    const PbrMesh & object           = params.m_static_meshes[i];
                    const IndexBuffer & index_buffer = params.m_index_buffers->at(object.m_index_buffer_id);
                    ray_descriptor_sets[2].set_t_byte_address_buffer(1,
                                                                     index_buffer.m_index_buffer,
                                                                     sizeof(uint32_t),
                                                                     object.m_num_indices,
                                                                     i,
                                                                     object.m_index_buffer_offset);
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
                if (i < params.m_static_meshes.size())
                {
                    const PbrMesh & object           = params.m_static_meshes[i];
                    const VertexBuffer & vertex_buffer = params.m_vertex_buffers->at(object.m_index_buffer_id);
                    ray_descriptor_sets[3].set_t_structured_buffer(0,
                                                                   vertex_buffer.m_vertex_buffer,
                                                                   sizeof(CompactVertex),
                                                                   object.m_num_vertices,
                                                                   i,
                                                                   object.m_vertex_buffer_offset);
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
            cmds.transition_texture(m_rt_result,
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
            ssao_descriptor_sets[0].set_t_texture(0, m_rt_result).set_s_sampler(0, m_sampler).update();

            // raster
            cmds.bind_graphics_descriptor_set(ssao_descriptor_sets);
            cmds.bind_vertex_buffer(params.m_vertex_buffers->at(0).m_vertex_buffer, sizeof(CompactVertex));
            cmds.bind_index_buffer(params.m_index_buffers->at(0).m_index_buffer, Gp::IndexType::Uint32);
            cmds.draw_instanced(3, 1, 0, 0);
            cmds.end_render_pass();

            // transition
            cmds.transition_texture(*ctx.m_swapchain_texture,
                                    Gp::TextureStateEnum::ColorAttachment,
                                    Gp::TextureStateEnum::Present);
            cmds.transition_texture(m_rt_result,
                                    Gp::TextureStateEnum::FragmentShaderVisible,
                                    Gp::TextureStateEnum::NonFragmentShaderVisible);

            cmds.end();
            cmds.submit(ctx.m_flight_fence, ctx.m_image_ready_semaphore, ctx.m_image_presentable_semaphores);
        }
    }
};