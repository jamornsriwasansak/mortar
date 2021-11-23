#pragma once

#include "rhi/rhi.h"
#include "scene_resource.h"
#include "shaders/raytrace_visualize_params.h"

struct RaytraceVisualizePass
{
    Rhi::RayTracingPipeline    m_rt_pipeline;
    Rhi::RayTracingShaderTable m_rt_sbt;
    Rhi::Buffer                m_cb_params;
    Rhi::Sampler               m_common_sampler;

#ifdef DEBUG_RayTraceVisualizePrintClickedInfo
    Rhi::Buffer m_debug_cb_params;
    Rhi::Buffer m_debug_print_buffer;
    Rhi::Buffer m_cpu_temp_buffer;
#endif

    RaytraceVisualizePass() {}

    RaytraceVisualizePass(const Rhi::Device & device, const ShaderManager & shader_manager)
    {
        init_or_reload(device, shader_manager);

        // constant params for rtvisualize
        m_cb_params = Rhi::Buffer("raytrace_visualize_cbparams",
                                  device,
                                  Rhi::BufferUsageEnum::ConstantBuffer,
                                  Rhi::MemoryUsageEnum::CpuToGpu,
                                  sizeof(RaytraceVisualizeCbParams));

        // sampler
        m_common_sampler = Rhi::Sampler(&device);

#ifdef DEBUG_RayTraceVisualizePrintClickedInfo
        // constant params for debug
        m_debug_cb_params = Rhi::Buffer(device,
                                        Rhi::BufferUsageEnum::ConstantBuffer,
                                        Rhi::MemoryUsageEnum::CpuToGpu,
                                        sizeof(RaytraceVisualizeDebugPrintCbParams),
                                        "raytrace_visualize_debug_cbparams");

        // print buffer
        m_debug_print_buffer =
            Rhi::Buffer(device,
                        Rhi::BufferUsageEnum::StorageBuffer | Rhi::BufferUsageEnum::TransferSrc,
                        Rhi::MemoryUsageEnum::GpuOnly,
                        sizeof(uint32_t) * DEBUG_RayTraceVisualizePrintChar4BufferSize,
                        "raytrace_visualize_debug_print_buffer");

        m_cpu_temp_buffer = Rhi::Buffer(device,
                                        Rhi::BufferUsageEnum::TransferDst,
                                        Rhi::MemoryUsageEnum::GpuToCpu,
                                        sizeof(uint32_t) * DEBUG_RayTraceVisualizePrintChar4BufferSize,
                                        "raytrace_visualize_debug_print_download_buffer");
#endif
    }

    void
    init_or_reload(const Rhi::Device & device, const ShaderManager & shader_manager)
    {
        // create pipeline for ssao
        Rhi::RayTracingPipelineConfig rt_config;

        // raygen
        const Rhi::ShaderSrc          raygen_shader(Rhi::ShaderStageEnum::RayGen,
                                           BASE_SHADER_DIR "raytrace_visualize.hlsl",
                                           "RayGen");
        [[maybe_unused]] const size_t raygen_id = rt_config.add_shader(raygen_shader);

        // miss
        const Rhi::ShaderSrc          miss_shader(Rhi::ShaderStageEnum::Miss,
                                         BASE_SHADER_DIR "raytrace_visualize.hlsl",
                                         "Miss");
        [[maybe_unused]] const size_t miss_id = rt_config.add_shader(miss_shader);

        // hitgroup
        const Rhi::ShaderSrc    hit_shader(Rhi::ShaderStageEnum::ClosestHit,
                                        BASE_SHADER_DIR "raytrace_visualize.hlsl",
                                        "ClosestHit");
        Rhi::RayTracingHitGroup hit_group;
        hit_group.m_closest_hit_id = rt_config.add_shader(hit_shader);

        // all raygen and all hit groups
        [[maybe_unused]] const size_t hitgroup_id = rt_config.add_hit_group(hit_group);

        m_rt_pipeline = Rhi::RayTracingPipeline(&device,
                                                rt_config,
                                                shader_manager,
                                                16,
                                                64,
                                                2,
                                                "raytrace_visualize_pipeline");
        m_rt_sbt = Rhi::RayTracingShaderTable(&device, m_rt_pipeline, "raytrace_visualize_sbt");
    }

    void
    run(Rhi::CommandList & cmd_list, const RenderContext & ctx, const Rhi::Texture & target_texture_buffer, const uint2 target_resolution)
    {
        bool       p_open     = true;
        static int rtvis_mode = 0;
        if (ImGui::Begin(typeid(*this).name(), &p_open))
        {
            ImGui::RadioButton("InstanceId", &rtvis_mode, static_cast<int>(RaytraceVisualizeModeEnum::ModeInstanceId));
            ImGui::RadioButton("BaseInstanceId",
                               &rtvis_mode,
                               static_cast<int>(RaytraceVisualizeModeEnum::ModeBaseInstanceId));
            ImGui::RadioButton("GeometryId", &rtvis_mode, static_cast<int>(RaytraceVisualizeModeEnum::ModeGeometryId));
            ImGui::RadioButton("TriangleId", &rtvis_mode, static_cast<int>(RaytraceVisualizeModeEnum::ModeTriangleId));
            ImGui::RadioButton("BaryCentricCoords",
                               &rtvis_mode,
                               static_cast<int>(RaytraceVisualizeModeEnum::ModeBaryCentricCoords));
            ImGui::RadioButton("Position", &rtvis_mode, static_cast<int>(RaytraceVisualizeModeEnum::ModePosition));
            ImGui::RadioButton("Geometry Normal",
                               &rtvis_mode,
                               static_cast<int>(RaytraceVisualizeModeEnum::ModeGeometryNormal));
            ImGui::RadioButton("Shading Normal",
                               &rtvis_mode,
                               static_cast<int>(RaytraceVisualizeModeEnum::ModeShadingNormal));
            ImGui::RadioButton("Texture Coord",
                               &rtvis_mode,
                               static_cast<int>(RaytraceVisualizeModeEnum::ModeTextureCoords));
            ImGui::RadioButton("Depth", &rtvis_mode, static_cast<int>(RaytraceVisualizeModeEnum::ModeDepth));
            ImGui::RadioButton("DiffuseReflectance",
                               &rtvis_mode,
                               static_cast<int>(RaytraceVisualizeModeEnum::ModeDiffuseReflectance));
            ImGui::RadioButton("SpecularReflectance",
                               &rtvis_mode,
                               static_cast<int>(RaytraceVisualizeModeEnum::ModeSpecularReflectance));
            ImGui::RadioButton("Roughness", &rtvis_mode, static_cast<int>(RaytraceVisualizeModeEnum::ModeRoughness));
        }
        ImGui::End();

        RaytraceVisualizeCbParams cb_params;
        CameraProperties          cam_props = ctx.m_fps_camera.get_camera_props();
        cb_params.m_camera_inv_proj         = inverse(cam_props.m_proj);
        cb_params.m_camera_inv_view         = inverse(cam_props.m_view);
        cb_params.m_mode                    = static_cast<RaytraceVisualizeModeEnum>(rtvis_mode);
        std::memcpy(m_cb_params.map(), &cb_params, sizeof(RaytraceVisualizeCbParams));
        m_cb_params.unmap();

        // setup descriptor spaces and bindings
#ifdef DEBUG_RayTraceVisualizePrintClickedInfo
        std::array<Rhi::DescriptorSet, 3> descriptor_sets;
#else
        std::array<Rhi::DescriptorSet, 2> descriptor_sets;
#endif

        // descriptor sets 0
        descriptor_sets[0] = Rhi::DescriptorSet(&ctx.m_device, m_rt_pipeline, &ctx.m_descriptor_pool, 0);
        descriptor_sets[0]
            .set_u_rw_texture(0, target_texture_buffer)
            .set_b_constant_buffer(0, m_cb_params)
            .update();

        // descriptor sets 1
        descriptor_sets[1] = Rhi::DescriptorSet(&ctx.m_device, m_rt_pipeline, &ctx.m_descriptor_pool, 1);
        for (size_t i = 0; i < std::min(ctx.m_scene_resource.m_d_textures.size(),
                                        size_t(EngineSetting::MaxNumBindlessTextures));
             i++)
        {
            descriptor_sets[1].set_t_texture(7, ctx.m_scene_resource.m_d_textures[i], i);
        }
        for (size_t i = ctx.m_scene_resource.m_d_textures.size(); i < EngineSetting::MaxNumBindlessTextures; i++)
        {
            descriptor_sets[1].set_t_texture(7, ctx.m_scene_resource.m_d_textures[0], i);
        }
        descriptor_sets[1]
            .set_s_sampler(0, m_common_sampler)
            .set_t_ray_tracing_accel(0, ctx.m_scene_resource.m_rt_tlas)
            .set_t_structured_buffer(1,
                                     ctx.m_scene_resource.m_d_base_instance_table,
                                     sizeof(BaseInstanceTableEntry),
                                     ctx.m_scene_resource.m_num_base_instance_table_entries)
            .set_t_structured_buffer(2,
                                     ctx.m_scene_resource.m_d_geometry_table,
                                     sizeof(GeometryTableEntry),
                                     ctx.m_scene_resource.m_num_geometry_table_entries)
            .set_t_structured_buffer(3,
                                     ctx.m_scene_resource.m_d_ibuf,
                                     sizeof(uint16_t),
                                     ctx.m_scene_resource.m_num_indices)
            .set_t_structured_buffer(4,
                                     ctx.m_scene_resource.m_d_vbuf_position,
                                     sizeof(float3),
                                     ctx.m_scene_resource.m_num_vertices)
            .set_t_structured_buffer(5,
                                     ctx.m_scene_resource.m_d_vbuf_packed,
                                     sizeof(CompactVertex),
                                     ctx.m_scene_resource.m_num_vertices)
            .set_t_structured_buffer(6, ctx.m_scene_resource.m_d_materials, sizeof(StandardMaterial), EngineSetting::MaxNumStandardMaterials)
            .update();

#ifdef DEBUG_RayTraceVisualizePrintClickedInfo
        // set debug cb params
        auto * debug_params = static_cast<RaytraceVisualizeDebugPrintCbParams *>(m_debug_cb_params.map());
        debug_params->m_selected_thread_id.x = static_cast<uint>(ImGui::GetMousePos().x);
        debug_params->m_selected_thread_id.y =
            static_cast<uint>(target_resolution.y - ImGui::GetMousePos().y);
        m_debug_cb_params.unmap();

        // descriptor set
        descriptor_sets[2] =
            Rhi::DescriptorSet(render_ctx.m_device, m_rt_pipeline, render_ctx.m_descriptor_pool, 2);
        descriptor_sets[2]
            .set_b_constant_buffer(0, m_debug_cb_params)
            .set_u_rw_structured_buffer(0, m_debug_print_buffer, sizeof(uint32_t), DEBUG_RayTraceVisualizePrintChar4BufferSize)
            .update();

        // display the debug result (from the previos frame)
        if (ImGui::Begin("DEBUG_RayTraceVisualizePrintClickedInfo", &p_open))
        {
            cmd_list.copy_buffer_region(m_cpu_temp_buffer, 0, m_debug_print_buffer, 0, sizeof(uint32_t) * DEBUG_RayTraceVisualizePrintChar4BufferSize);

            // print the result from the previous frame.
            // might have race condition but since it's for debugging a single pixel, it should be fine.
            const char * mapped_debug_str = static_cast<char *>(m_cpu_temp_buffer.map());
            ImGui::Text(mapped_debug_str);
            m_cpu_temp_buffer.unmap();
        }
        ImGui::End();
#endif

        cmd_list.bind_raytrace_pipeline(m_rt_pipeline);
        cmd_list.bind_raytrace_descriptor_set(descriptor_sets);
        cmd_list.trace_rays(m_rt_sbt, target_resolution.x, target_resolution.y);
    }
};