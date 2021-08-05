#pragma once

#include "assetmanager.h"
#include "common/camera.h"
#include "graphicsapi/graphicsapi.h"
#include "render/rendercontext.h"
#include "render/renderer.h"

#include "assetbrowser.h"
#include "assetbrowserquick.h"

struct MainLoop
{
    Gp::Device * m_device = nullptr;
    Window *     m_window = nullptr;

    Gp::Swapchain                   m_swapchain;
    std::vector<Gp::Texture>        m_swapchain_textures;
    std::vector<Gp::Fence>          m_flight_fences;
    std::vector<Gp::CommandPool>    m_graphics_command_pools;
    std::vector<Gp::CommandPool>    m_compute_command_pools;
    std::vector<Gp::CommandPool>    m_transfer_command_pools;
    std::vector<Gp::DescriptorPool> m_descriptor_pools;
    std::vector<Gp::Semaphore>      m_image_ready_semaphores;
    std::vector<Gp::Semaphore>      m_image_presentable_semaphore;
    size_t                          m_swapchain_length = 0;
    size_t                          m_num_flights      = 0;

    Gp::StagingBufferManager m_staging_buffer_manager;
    // TODO:: replace asset pool completely with scene
    AssetPool m_asset_pool;
    Scene     m_scene;
    FpsCamera m_camera;
    Renderer  m_renderer;
    int2      m_swapchain_resolution = int2(0, 0);

    std::vector<StandardObject> m_static_objects;

    Gp::Buffer  m_dummy_buffer;
    Gp::Buffer  m_dummy_index_buffer;
    Gp::Texture m_dummy_texture;

    Gp::ImGuiRenderPass m_imgui_render_pass;
    AssetBrowserQuick   m_asset_browser;

    MainLoop() {}

    MainLoop(Gp::Device * device, Window * window, const size_t num_flights)
    : m_device(device), m_window(window), m_num_flights(num_flights)
    {
        const int2 resolution = m_window->get_resolution();
        m_camera              = FpsCamera(float3(10.0f, 10.0f, 10.0f),
                             float3(0, 0, 0),
                             float3(0, 1, 0),
                             radians(60.0f),
                             float(resolution.x) / float(resolution.y));
        m_scene               = Scene(*device);
    }

    void
    init_or_resize_swapchain()
    {
        // force all unique ptr to free
        m_swapchain = Gp::Swapchain();

        // create swapchain
        m_swapchain        = Gp::Swapchain(m_device, *m_window, "main_swapchain");
        m_swapchain_length = m_swapchain.m_num_images;
        m_swapchain_textures.resize(m_swapchain_length);
        for (size_t i = 0; i < m_swapchain.m_num_images; i++)
        {
            m_swapchain_textures[i] = Gp::Texture(m_device, m_swapchain, i);
        }

        // initalize camera
        m_swapchain_resolution = m_window->get_resolution();
        m_camera.m_aspect_ratio =
            static_cast<float>(m_swapchain_resolution.x) / static_cast<float>(m_swapchain_resolution.y);
    }

    void
    init()
    {
        init_or_resize_swapchain();

        // create in flight fence
        m_flight_fences.resize(m_num_flights);
        for (int i = 0; i < m_num_flights; i++)
        {
            m_flight_fences[i] = Gp::Fence(m_device, "flight_fence" + std::to_string(i));
        }

        // create command pool and descriptor pool
        m_graphics_command_pools.resize(m_num_flights);
        m_compute_command_pools.resize(m_num_flights);
        m_transfer_command_pools.resize(m_num_flights);
        m_descriptor_pools.resize(m_num_flights);
        m_image_ready_semaphores.resize(m_num_flights);
        m_image_presentable_semaphore.resize(m_num_flights);
        for (size_t i = 0; i < m_num_flights; i++)
        {
            m_graphics_command_pools[i] = Gp::CommandPool(m_device, Gp::CommandQueueType::Graphics);
            m_compute_command_pools[i]  = Gp::CommandPool(m_device, Gp::CommandQueueType::Compute);
            m_transfer_command_pools[i] = Gp::CommandPool(m_device, Gp::CommandQueueType::Transfer);
            m_descriptor_pools[i]       = Gp::DescriptorPool(m_device);
            m_image_ready_semaphores[i] = Gp::Semaphore(m_device);
            m_image_presentable_semaphore[i] = Gp::Semaphore(m_device);
        }

        // staging buffer manager
        m_staging_buffer_manager = Gp::StagingBufferManager(m_device, "main_staging_buffer_manager");
        m_asset_pool             = AssetPool(m_device, &m_staging_buffer_manager);

        // initialize renderer
        m_renderer.init(m_device, m_swapchain_resolution, m_swapchain_textures);

        // init dummy buffers and texture
        m_dummy_buffer  = Gp::Buffer(m_device,
                                    Gp::BufferUsageEnum::VertexBuffer | Gp::BufferUsageEnum::IndexBuffer |
                                        Gp::BufferUsageEnum::StorageBuffer,
                                    Gp::MemoryUsageEnum::GpuOnly,
                                    sizeof(uint32_t),
                                    nullptr,
                                    nullptr,
                                    "dummy_vertex_buffer");
        m_dummy_texture = Gp::Texture(m_device,
                                      Gp::TextureUsageEnum::Sampled,
                                      Gp::TextureStateEnum::AllShaderVisible,
                                      Gp::FormatEnum::R8G8B8A8_UNorm,
                                      int2(1, 1),
                                      nullptr,
                                      nullptr,
                                      float4(0.0f, 0.0f, 0.0f, 0.0f),
                                      "dummy_texture");

        // setup dear imgui
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();
        ImGuiIO & io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.Fonts->AddFontFromFileTTF("resources/fonts/Roboto/Roboto-Medium.ttf", 15);
        m_imgui_render_pass = Gp::ImGuiRenderPass(m_device, *m_window, m_swapchain, m_num_flights);

        m_asset_browser.init();
    }

    void
    loop(const size_t i_flight)
    {
        GlfwHandler::Inst().poll_events();

        int2 current_resolution = m_window->get_resolution();
        if (current_resolution.y == 0)
        {
            return;
        }

        m_camera.update(m_window, std::min(m_window->m_stop_watch.m_average_frame_time * 0.01f, 1.0f));

        // wait for resource in this flight to be ready
        m_flight_fences[i_flight].wait();

        bool reload_shader = false;
        if (m_window->m_R.m_event == KeyEventEnum::JustPress &&
            m_window->m_LEFT_CONTROL.m_event == KeyEventEnum::Hold)
        {
            reload_shader = true;
            for (size_t i = 0; i < m_num_flights; i++)
            {
                m_flight_fences[i].wait();
            }
        }

        // reset all resource
        m_flight_fences[i_flight].reset();
        m_graphics_command_pools[i_flight].reset();
        m_descriptor_pools[i_flight].reset();

        // update image index
        m_swapchain.update_image_index(&m_image_ready_semaphores[i_flight]);

        // setup context
        RenderContext ctx;
        ctx.m_device                      = m_device;
        ctx.m_graphics_command_pool       = &m_graphics_command_pools[i_flight];
        ctx.m_descriptor_pool             = &m_descriptor_pools[i_flight];
        ctx.m_image_ready_semaphore       = &m_image_ready_semaphores[i_flight];
        ctx.m_image_presentable_semaphore = &m_image_presentable_semaphore[i_flight];
        ctx.m_flight_fence                = &m_flight_fences[i_flight];
        ctx.m_flight_index                = i_flight;
        ctx.m_image_index                 = m_swapchain.m_image_index;
        ctx.m_swapchain_texture           = &m_swapchain_textures[m_swapchain.m_image_index];
        ctx.m_staging_buffer_manager      = &m_staging_buffer_manager;
        ctx.m_dummy_buffer                = &m_dummy_buffer;
        ctx.m_dummy_texture               = &m_dummy_texture;
        ctx.m_imgui_render_pass           = &m_imgui_render_pass;

        RenderParams render_params;
        render_params.m_resolution           = m_swapchain_resolution;
        render_params.m_asset_pool           = &m_asset_pool;
        render_params.m_scene                = &m_scene;
        render_params.m_static_objects       = &m_static_objects;
        render_params.m_is_static_mesh_dirty = false;
        render_params.m_fps_camera           = &m_camera;
        render_params.m_is_shaders_dirty     = reload_shader;
        render_params.m_should_imgui_drawn   = false;

        if (render_params.m_should_imgui_drawn)
        {
            m_imgui_render_pass.new_frame();
        }
        // m_asset_browser.loop();
        m_renderer.loop(ctx, render_params);
        if (render_params.m_should_imgui_drawn)
        {
            m_imgui_render_pass.end_frame();
        }

        if (!m_swapchain.present(&m_image_presentable_semaphore[i_flight]))
        {
            // wait until all resource are not used
            for (size_t i = 0; i < m_flight_fences.size(); i++)
            {
                m_flight_fences[i].wait();
            }

            init_or_resize_swapchain();
            m_imgui_render_pass.init_or_resize_framebuffer(m_device, m_swapchain);
            m_renderer.init_or_resize_resolution(m_device, m_window->get_resolution(), m_swapchain_textures);
            m_renderer.init_shaders(m_device, false);
        }
        m_window->update();
    }

    void
    run()
    {
        size_t i_flight = 0;

        std::vector<StandardObject> sponza_mesh =
            m_asset_pool.add_standard_meshes("scenes/sponza/sponza.obj");
        m_static_objects.insert(m_static_objects.end(), sponza_mesh.begin(), sponza_mesh.end());

        int white_tex_id =
            m_asset_pool.add_constant_texture(std::array<uint8_t, 4>{ static_cast<uint8_t>(255),
                                                                      static_cast<uint8_t>(255),
                                                                      static_cast<uint8_t>(255),
                                                                      static_cast<uint8_t>(255) });

        StandardEmissive standard_emissive;
        standard_emissive.m_emissive_tex_id = white_tex_id;
        standard_emissive.m_emissive_scale  = 100.0f;
        int emissive_id                     = m_asset_pool.add_standard_emissive(standard_emissive);

        std::vector<StandardObject> box_mesh =
            m_asset_pool.add_standard_meshes("scenes/cube/cube.obj", false, false);
        box_mesh[0].m_emissive_id = emissive_id;
        m_static_objects.insert(m_static_objects.end(), box_mesh.begin(), box_mesh.end());

        //m_scene.add_models("scenes/sponza/sponza.obj", m_staging_buffer_manager);

        // int2 salle = m_asset_manager.add_standard_object("salle_de_bain/salle_de_bain.obj");

        while (!m_window->should_close_window())
        {
            loop(i_flight);
            i_flight = (i_flight + 1) % m_num_flights;
        }

        m_imgui_render_pass.shut_down();

        for (size_t i = 0; i < m_num_flights; i++)
        {
            m_flight_fences[i].wait();
        }
    }
};