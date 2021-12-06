#pragma once

#include "core/camera.h"
#include "render/render_context.h"
#include "render/renderer.h"
#include "rhi/rhi.h"
#include "rhi/shadercompiler/shader_binary_manager.h"

#include "assetbrowser.h"
#include "assetbrowserquick.h"

#include <ranges>

struct MainLoop
{
    struct PerFlightResource
    {
        Rhi::Fence          m_flight_fence;
        Rhi::CommandPool    m_graphics_command_pool;
        Rhi::CommandPool    m_compute_command_pool;
        Rhi::CommandPool    m_transfer_command_pool;
        Rhi::DescriptorPool m_descriptor_pool;
        Rhi::Semaphore      m_image_ready_semaphore;
        Rhi::Semaphore      m_image_presentable_semaphore;

        PerFlightResource(const std::string & name, const Rhi::Device & device)
        : m_flight_fence(name + "_flight_fence", device),
          m_graphics_command_pool(name + "_graphics_command_pool", device, Rhi::CommandQueueType::Graphics),
          m_compute_command_pool(name + "_graphics_command_pool", device, Rhi::CommandQueueType::Compute),
          m_transfer_command_pool(name + "_graphics_command_pool", device, Rhi::CommandQueueType::Transfer),
          m_descriptor_pool(name + "_descriptor_pool", device),
          m_image_ready_semaphore(name + "_image_read_semaphore", device),
          m_image_presentable_semaphore(name + "_image_presentable_semaphore", device)
        {
        }

        void
        wait()
        {
            m_flight_fence.wait();
        }

        void
        reset()
        {
            m_flight_fence.reset();
            m_graphics_command_pool.reset();
            m_compute_command_pool.reset();
            m_transfer_command_pool.reset();
            m_descriptor_pool.reset();
        }
    };

    struct PerSwapResource
    {
        Rhi::Texture m_swapchain_texture;
        Rhi::Fence * m_swapchain_image_fence;

        PerSwapResource(const std::string & name, const Rhi::Device & device, const Rhi::Swapchain & swapchain, const size_t i_image)
        : m_swapchain_texture(name + "_swapchain_texture", device, swapchain, i_image, float4(0.0f, 0.0f, 0.0f, 0.0f)),
          m_swapchain_image_fence(nullptr)
        {
        }
    };

    Window & m_window;

    Rhi::Device &             m_device;
    Rhi::Swapchain &          m_swapchain;
    Rhi::ImGuiRenderPass      m_imgui_render_pass;
    Rhi::StagingBufferManager m_staging_buffer_manager;

    std::vector<PerFlightResource> m_per_flight_resources;
    std::vector<PerSwapResource>   m_per_swap_resources;
    size_t                         m_num_flights = 0;

    ShaderBinaryManager & m_shader_binary_manager;
    SceneResource         m_scene_resource;
    FpsCamera             m_camera;
    int2                  m_swapchain_resolution = int2(0, 0);

    AssetBrowserQuick m_asset_browser;

    Renderer m_renderer;

    bool m_is_reload_shader_needed = true;

    MainLoop(Rhi::Device &         device,
             Window &              window,
             const size_t          num_flights,
             ShaderBinaryManager & shader_binary_manager,
             Rhi::Swapchain &      swapchain)
    : m_device(device),
      m_window(window),
      m_swapchain_resolution(window.get_resolution()),
      m_swapchain(swapchain),
      m_num_flights(num_flights),
      m_scene_resource(device),
      m_shader_binary_manager(shader_binary_manager),
      m_per_flight_resources(construct_per_flight_resources("main_flight_resource", device, num_flights)),
      m_per_swap_resources(construct_per_swap_resources("main_swap_resources", device, swapchain)),
      m_imgui_render_pass(device, window, swapchain),
      m_camera(float3(10.0f, 10.0f, 10.0f),
               float3(0.0f, 0.0f, 0.0f),
               float3(0.0f, 1.0f, 0.0f),
               radians(60.0f),
               static_cast<float>(window.get_resolution().x) /
                   static_cast<float>(window.get_resolution().y)),
      m_staging_buffer_manager("main_staging_buffer_manager", device),
      m_renderer(device, shader_binary_manager, get_swapchain_textures(m_per_swap_resources), window.get_resolution(), num_flights)
    {
    }

    std::vector<PerFlightResource>
    construct_per_flight_resources(const std::string & name, const Rhi::Device & device, const size_t num_flights)
    {
        std::vector<PerFlightResource> result;
        result.reserve(num_flights);
        for (size_t i_flight = 0; i_flight < num_flights; i_flight++)
        {
            result.emplace_back(name + "_flight" + std::to_string(i_flight), device);
        }
        return result;
    }

    std::vector<PerSwapResource>
    construct_per_swap_resources(const std::string & name, const Rhi::Device & device, const Rhi::Swapchain & swapchain)
    {
        std::vector<PerSwapResource> result;
        result.reserve(swapchain.m_num_images);
        for (size_t i_swap = 0; i_swap < swapchain.m_num_images; i_swap++)
        {
            result.emplace_back(name + "_swap" + std::to_string(i_swap), device, swapchain, i_swap);
        }
        return result;
    }

    std::vector<const Rhi::Texture *>
    get_swapchain_textures(const std::span<PerSwapResource> & per_swap_resources)
    {
        std::vector<const Rhi::Texture *> result;
        result.reserve(per_swap_resources.size());
        for (const PerSwapResource & item : per_swap_resources)
        {
            result.emplace_back(&item.m_swapchain_texture);
        }
        return result;
    }

    void
    resize_swapchain()
    {
        Logger::Info(__FUNCTION__, " start resizing swapchain");
        // we flush command stuck in the queue
        for (PerFlightResource & per_flight_resource : m_per_flight_resources)
        {
            per_flight_resource.m_flight_fence.reset();
            per_flight_resource.m_graphics_command_pool.flush(per_flight_resource.m_flight_fence);
        }
        for (PerFlightResource & per_flight_resource : m_per_flight_resources)
        {
            per_flight_resource.wait();
        }
        m_swapchain.resize_to_window(m_device, m_window);
    }

    void
    loop(const size_t i_flight)
    {
        GlfwHandler::Inst().poll_events();


        int2 current_resolution = m_window.get_resolution();
        if (current_resolution.y == 0)
        {
            return;
        }

        // wait for resource in this flight to be ready
        PerFlightResource & per_flight_resource = m_per_flight_resources[i_flight];
        per_flight_resource.wait();

        bool reload_shader = m_is_reload_shader_needed;
        if (m_window.m_R.m_event == KeyEventEnum::JustPress && m_window.m_LEFT_CONTROL.m_event == KeyEventEnum::Hold)
        {
            reload_shader = true;
        }
        m_is_reload_shader_needed = false;

        // reset all resource
        per_flight_resource.reset();

        // update image index
        m_swapchain.update_image_index(&per_flight_resource.m_image_ready_semaphore);

        PerSwapResource & per_swap_resource = m_per_swap_resources[m_swapchain.m_image_index];

        // wait until the image we want is draw is finished drawing
        if (per_swap_resource.m_swapchain_image_fence)
        {
            per_swap_resource.m_swapchain_image_fence->wait();
            per_swap_resource.m_swapchain_image_fence = &per_flight_resource.m_flight_fence;
        }

        // context parameters for each frame
        RenderContext ctx(m_device,
                          per_flight_resource.m_graphics_command_pool,
                          per_flight_resource.m_descriptor_pool,
                          per_flight_resource.m_image_ready_semaphore,
                          per_flight_resource.m_image_presentable_semaphore,
                          per_flight_resource.m_flight_fence,
                          per_swap_resource.m_swapchain_texture,
                          m_staging_buffer_manager,
                          m_imgui_render_pass,
                          m_shader_binary_manager,
                          i_flight,
                          m_swapchain.m_image_index,
                          m_swapchain_resolution,
                          m_scene_resource,
                          m_camera,
                          reload_shader,
                          true);

        if (ctx.m_should_imgui_drawn)
        {
            m_imgui_render_pass.new_frame();
        }

        const bool is_imgui_used =
            ImGui::IsAnyItemActive() || ImGui::IsAnyItemFocused() || ImGui::IsAnyItemHovered();
        m_camera.update(&m_window, std::min(m_window.m_stop_watch.m_average_frame_time * 0.01f, 1.0f), !is_imgui_used);

        m_renderer.loop(ctx);
        if (ctx.m_should_imgui_drawn)
        {
            m_imgui_render_pass.end_frame();
        }

        if (!m_swapchain.present(&per_flight_resource.m_image_presentable_semaphore) ||
            any(notEqual(current_resolution, m_swapchain_resolution)))
        {
            for (PerFlightResource & resource : m_per_flight_resources)
            {
                resource.wait();
            }

            resize_swapchain();
            m_per_swap_resources = construct_per_swap_resources("main_swap_resources", m_device, m_swapchain);
            m_renderer.resize(m_device,
                              m_window.get_resolution(),
                              m_shader_binary_manager,
                              get_swapchain_textures(m_per_swap_resources),
                              m_num_flights);
            m_imgui_render_pass.init_or_resize_framebuffer(m_device, m_swapchain);
            m_is_reload_shader_needed = true;

            // initialize camera
            m_swapchain_resolution  = m_window.get_resolution();
            m_camera.m_aspect_ratio = static_cast<float>(m_swapchain_resolution.x) /
                                      static_cast<float>(m_swapchain_resolution.y);
        }
        m_window.update();
    }

    void
    run()
    {
        size_t i_flight = 0;

        const urange32_t sponza_geometries =
            m_scene_resource.add_geometries("scenes/sponza/sponza.obj", m_staging_buffer_manager);
        // m_scene_resource.add_geometries("tmp/Exterior/Exterior.obj", m_staging_buffer_manager);
        std::array<urange32_t, 1> ranges             = { sponza_geometries };
        size_t                    sponza_instance_id = m_scene_resource.add_base_instance(ranges);
        // m_scene.add_render_object(&m_scene.m_scene_graph_root, "scenes/cube/cube.obj", m_staging_buffer_manager);

        SceneDesc scene_desc;
        for (size_t j = 0; j < 1; j++)
        {
            for (size_t i = 0; i < 1; i++)
            {
                const float4x4 scale = glm::scale(glm::identity<float4x4>(), float3(1.0f));
                const float4x4 translate =
                    glm::translate(glm::identity<float4x4>(), float3(40.0f * j, 0.0f, 40.0f * i));
                SceneInstance instance2 = { sponza_instance_id, translate * scale };
                scene_desc.m_instances.push_back(instance2);
            }
        }
        m_scene_resource.commit(scene_desc, m_staging_buffer_manager);

        // int2 salle = m_asset_manager.add_standard_object("salle_de_bain/salle_de_bain.obj");

        m_window.m_stop_watch.reset();
        while (!m_window.should_close_window())
        {
            loop(i_flight);
            i_flight = (i_flight + 1) % m_num_flights;
        }

        // wait until all resource are not used
        for (PerFlightResource & per_flight_resource : m_per_flight_resources)
        {
            per_flight_resource.wait();
        }

        m_imgui_render_pass.shut_down();
    }
};