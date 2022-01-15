#pragma once

#include "application/scene_resource.h"
#include "application/per_flight_resource.h"
#include "application/per_swap_resource.h"
#include "rhi/rhi.h"
#include "rhi/shadercompiler/shader_binary_manager.h"

struct RenderContext
{
    Rhi::Device &               m_device;
    PerFlightResource &         m_per_flight_resource;
    PerSwapResource &           m_per_swap_resource;
    Rhi::StagingBufferManager & m_staging_buffer_manager;
    Rhi::ImGuiRenderPass &      m_imgui_render_pass;
    ShaderBinaryManager &       m_shader_binary_manager;
    size_t                      m_flight_index;
    size_t                      m_image_index;
    int2                        m_resolution;
    SceneResource &             m_scene_resource;
    FpsCamera &                 m_fps_camera;
    bool                        m_is_shaders_dirty;
    bool                        m_should_imgui_drawn;
    bool                        m_do_profile;

    RenderContext(Rhi::Device &               device,
                  PerFlightResource &         per_flight_resource,
                  PerSwapResource &           per_swap_resource,
                  Rhi::StagingBufferManager & staging_buffer_manager,
                  Rhi::ImGuiRenderPass &      imgui_render_pass,
                  ShaderBinaryManager &       shader_binary_manager,
                  size_t                      flight_index,
                  size_t                      image_index,
                  int2                        resolution,
                  SceneResource &             scene_resource,
                  FpsCamera &                 fps_camera,
                  const bool                  is_shaders_dirty,
                  const bool                  should_imgui_drawn,
                  const bool                  do_profile)
    : m_device(device),
      m_per_flight_resource(per_flight_resource),
      m_per_swap_resource(per_swap_resource),
      m_staging_buffer_manager(staging_buffer_manager),
      m_imgui_render_pass(imgui_render_pass),
      m_shader_binary_manager(shader_binary_manager),
      m_flight_index(flight_index),
      m_image_index(image_index),
      m_resolution(resolution),
      m_scene_resource(scene_resource),
      m_fps_camera(fps_camera),
      m_is_shaders_dirty(is_shaders_dirty),
      m_should_imgui_drawn(should_imgui_drawn),
      m_do_profile(do_profile)
    {
    }
};