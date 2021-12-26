#pragma once

#include "core/gui_event_coordinator.h"
#include "rhi/rhi.h"
#include "shaders/cpp_compatible.h"
#include "shaders/ray_trace_gbuffer_generate_params.h"

struct RayTraceVisualizeUiParams
{
    int m_rtvis_mode = 0;

    RayTraceVisualizeUiParams() {}

    void
    draw_gui(GuiEventCoordinator & gui_event_flags)
    {
        if (gui_event_flags.m_display_raytrace_visualize_menu)
        {
            if (ImGui::Begin(typeid(*this).name(), &gui_event_flags.m_display_raytrace_visualize_menu))
            {
                ImGui::RadioButton("InstanceId",
                                   &m_rtvis_mode,
                                   static_cast<int>(RaytraceVisualizeModeEnum::ModeInstanceId));
                ImGui::RadioButton("BaseInstanceId",
                                   &m_rtvis_mode,
                                   static_cast<int>(RaytraceVisualizeModeEnum::ModeBaseInstanceId));
                ImGui::RadioButton("GeometryId",
                                   &m_rtvis_mode,
                                   static_cast<int>(RaytraceVisualizeModeEnum::ModeGeometryId));
                ImGui::RadioButton("TriangleId",
                                   &m_rtvis_mode,
                                   static_cast<int>(RaytraceVisualizeModeEnum::ModeTriangleId));
                ImGui::RadioButton("BaryCentricCoords",
                                   &m_rtvis_mode,
                                   static_cast<int>(RaytraceVisualizeModeEnum::ModeBaryCentricCoords));
                ImGui::RadioButton("Position", &m_rtvis_mode, static_cast<int>(RaytraceVisualizeModeEnum::ModePosition));
                ImGui::RadioButton("Geometry Normal",
                                   &m_rtvis_mode,
                                   static_cast<int>(RaytraceVisualizeModeEnum::ModeGeometryNormal));
                ImGui::RadioButton("Shading Normal",
                                   &m_rtvis_mode,
                                   static_cast<int>(RaytraceVisualizeModeEnum::ModeShadingNormal));
                ImGui::RadioButton("Texture Coord",
                                   &m_rtvis_mode,
                                   static_cast<int>(RaytraceVisualizeModeEnum::ModeTextureCoords));
                ImGui::RadioButton("Depth", &m_rtvis_mode, static_cast<int>(RaytraceVisualizeModeEnum::ModeDepth));
                ImGui::RadioButton("DiffuseReflectance",
                                   &m_rtvis_mode,
                                   static_cast<int>(RaytraceVisualizeModeEnum::ModeDiffuseReflectance));
                ImGui::RadioButton("SpecularReflectance",
                                   &m_rtvis_mode,
                                   static_cast<int>(RaytraceVisualizeModeEnum::ModeSpecularReflectance));
                ImGui::RadioButton("Roughness",
                                   &m_rtvis_mode,
                                   static_cast<int>(RaytraceVisualizeModeEnum::ModeRoughness));
            }
            ImGui::End();
        }
    }
};

struct RayTraceGbufferGeneratePass
{
    Rhi::RayTracingPipeline    m_rt_pipeline;
    Rhi::RayTracingShaderTable m_rt_sbt;
    Rhi::Buffer                m_cb_params;
    Rhi::Sampler               m_common_sampler;

    RayTraceGbufferGeneratePass(const Rhi::Device & device, const ShaderBinaryManager & shader_binary_manager)
    : m_rt_pipeline("ray_trace_visualize_pipeline",
                    device,
                    get_ray_trace_visualize_pipeline_config(),
                    shader_binary_manager,
                    16,
                    64,
                    1),
      m_rt_sbt("ray_trace_visualize_sbt", device, m_rt_pipeline),
      m_common_sampler("ray_trace_visualize_sampler", device),
      m_cb_params("ray_trace_visualize_cbparams",
                  device,
                  Rhi::BufferUsageEnum::ConstantBuffer,
                  Rhi::MemoryUsageEnum::CpuToGpu,
                  sizeof(RaytraceVisualizeCbParams))
    {
    }

    Rhi::RayTracingPipelineConfig
    get_ray_trace_visualize_pipeline_config() const
    {
        // create pipeline for ssao
        Rhi::RayTracingPipelineConfig rt_config;

        // raygen
        const Rhi::ShaderSrc          raygen_shader(Rhi::ShaderStageEnum::RayGen,
                                           BASE_SHADER_DIR "ray_trace_gbuffer_generate.hlsl",
                                           "RayGen");
        [[maybe_unused]] const size_t raygen_id = rt_config.add_shader(raygen_shader);

        // miss
        const Rhi::ShaderSrc          miss_shader(Rhi::ShaderStageEnum::Miss,
                                         BASE_SHADER_DIR "ray_trace_gbuffer_generate.hlsl",
                                         "Miss");
        [[maybe_unused]] const size_t miss_id = rt_config.add_shader(miss_shader);

        // hitgroup
        const Rhi::ShaderSrc    hit_shader(Rhi::ShaderStageEnum::ClosestHit,
                                        BASE_SHADER_DIR "ray_trace_gbuffer_generate.hlsl",
                                        "ClosestHit");
        Rhi::RayTracingHitGroup hit_group;
        hit_group.m_closest_hit_id = rt_config.add_shader(hit_shader);

        // all raygen and all hit groups
        [[maybe_unused]] const size_t hitgroup_id = rt_config.add_hit_group(hit_group);

        return rt_config;
    }

    void
    render(Rhi::CommandBuffer &              cmd_buffer,
           const RenderContext &             ctx,
           const Rhi::Texture &              target_texture_buffer,
           const RayTraceVisualizeUiParams & params,
           const uint2                       target_resolution)
    {
        RaytraceVisualizeCbParams cb_params;
        CameraProperties          cam_props = ctx.m_fps_camera.get_camera_props();
        cb_params.m_camera_inv_proj         = inverse(cam_props.m_proj);
        cb_params.m_camera_inv_view         = inverse(cam_props.m_view);
        cb_params.m_mode = static_cast<RaytraceVisualizeModeEnum>(params.m_rtvis_mode);
        std::memcpy(m_cb_params.map(), &cb_params, sizeof(RaytraceVisualizeCbParams));
        m_cb_params.unmap();

        // setup descriptor spaces and bindings
        std::array<Rhi::DescriptorSet, 2> descriptor_sets = {
            Rhi::DescriptorSet(ctx.m_device, m_rt_pipeline, ctx.m_per_flight_resource.m_descriptor_pool, 0),
            Rhi::DescriptorSet(ctx.m_device, m_rt_pipeline, ctx.m_per_flight_resource.m_descriptor_pool, 1)
        };

        RayTraceVisualizeRegisters registers(descriptor_sets);
        registers.u_output.set(target_texture_buffer);
        registers.u_cbparams.set(m_cb_params);
        registers.u_sampler.set(m_common_sampler);
        registers.u_scene_bvh.set(ctx.m_scene_resource.m_rt_tlas);
        registers.u_base_instance_table.set(ctx.m_scene_resource.m_d_base_instance_table);
        registers.u_geometry_table.set(ctx.m_scene_resource.m_d_geometry_table);
        registers.u_indices.set(ctx.m_scene_resource.m_d_ibuf);
        registers.u_positions.set(ctx.m_scene_resource.m_d_vbuf_position);
        registers.u_compact_vertices.set(ctx.m_scene_resource.m_d_vbuf_packed);
        registers.u_materials.set(ctx.m_scene_resource.m_d_materials);
        for (size_t i = 0; i < ctx.m_scene_resource.m_d_textures.size(); i++)
        {
            registers.u_textures.set(ctx.m_scene_resource.m_d_textures[i], i);
        }

        descriptor_sets[0].update();
        descriptor_sets[1].update();

        cmd_buffer.bind_ray_trace_pipeline(m_rt_pipeline);
        cmd_buffer.bind_ray_trace_descriptor_set(descriptor_sets);
        cmd_buffer.trace_rays(m_rt_sbt, target_resolution.x, target_resolution.y);
    }
};