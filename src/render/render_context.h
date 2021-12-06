#pragma once

#include "rhi/rhi.h"
#include "rhi/shadercompiler/shader_binary_manager.h"
#include "scene_resource.h"

struct RenderContext
{
    Rhi::Device &               m_device;
    Rhi::CommandPool &          m_graphics_command_pool;
    Rhi::DescriptorPool &       m_descriptor_pool;
    Rhi::Semaphore &            m_image_ready_semaphore;
    Rhi::Semaphore &            m_image_presentable_semaphore;
    Rhi::Fence &                m_flight_fence;
    Rhi::Texture &              m_swapchain_texture;
    Rhi::StagingBufferManager & m_staging_buffer_manager;
    Rhi::ImGuiRenderPass &      m_imgui_render_pass;
    ShaderBinaryManager &             m_shader_binary_manager;
    size_t                      m_flight_index;
    size_t                      m_image_index;
    int2                        m_resolution;
    SceneResource &             m_scene_resource;
    FpsCamera &                 m_fps_camera;
    bool                        m_is_shaders_dirty;
    bool                        m_should_imgui_drawn;

    RenderContext(Rhi::Device &               device,
                  Rhi::CommandPool &          graphics_command_pool,
                  Rhi::DescriptorPool &       descriptor_pool,
                  Rhi::Semaphore &            image_ready_semaphore,
                  Rhi::Semaphore &            image_presentable_semaphore,
                  Rhi::Fence &                flight_fence,
                  Rhi::Texture &              swapchain_texture,
                  Rhi::StagingBufferManager & staging_buffer_manager,
                  Rhi::ImGuiRenderPass &      imgui_render_pass,
                  ShaderBinaryManager &             shader_binary_manager,
                  size_t                      flight_index,
                  size_t                      image_index,
                  int2                        resolution,
                  SceneResource &             scene_resource,
                  FpsCamera &                 fps_camera,
                  bool                        is_shaders_dirty,
                  bool                        should_imgui_drawn)
    : m_device(device),
      m_graphics_command_pool(graphics_command_pool),
      m_descriptor_pool(descriptor_pool),
      m_image_ready_semaphore(image_ready_semaphore),
      m_image_presentable_semaphore(image_presentable_semaphore),
      m_flight_fence(flight_fence),
      m_swapchain_texture(swapchain_texture),
      m_staging_buffer_manager(staging_buffer_manager),
      m_imgui_render_pass(imgui_render_pass),
      m_shader_binary_manager(shader_binary_manager),
      m_flight_index(flight_index),
      m_image_index(image_index),
      m_resolution(resolution),
      m_scene_resource(scene_resource),
      m_fps_camera(fps_camera),
      m_is_shaders_dirty(is_shaders_dirty),
      m_should_imgui_drawn(should_imgui_drawn)
    {
    }
};