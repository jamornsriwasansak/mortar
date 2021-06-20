#pragma once

#include "assetmanager.h"
#include "common/camera.h"
#include "graphicsapi/graphicsapi.h"
#include "render/rendercontext.h"
#include "render/renderer.h"

struct MainLoop
{
    Gp::Device * m_device = nullptr;
    Window *     m_window = nullptr;

    Gp::Swapchain                   m_swapchain;
    std::vector<Gp::Texture>        m_swapchain_textures;
    std::vector<Gp::Fence>          m_flight_fences;
    std::vector<Gp::CommandPool>    m_graphics_command_pool;
    std::vector<Gp::DescriptorPool> m_descriptor_pools;
    std::vector<Gp::Semaphore>      m_image_ready_semaphores;
    std::vector<Gp::Semaphore>      m_image_presentable_semaphores;
    size_t                          m_swapchain_length = 0;
    size_t                          m_num_flights      = 0;

    Gp::StagingBufferManager m_staging_buffer_manager;
    AssetManager             m_asset_manager;
    FpsCamera                m_camera;
    Renderer                 m_renderer;

    Gp::Buffer  m_dummy_buffer;
    Gp::Buffer  m_dummy_index_buffer;
    Gp::Texture m_dummy_texture;

    MainLoop() {}

    MainLoop(Gp::Device * device, Window * window, const size_t num_flights)
    : m_device(device), m_window(window), m_num_flights(num_flights)
    {
    }

    void
    init()
    {
        const int2 resolution = m_window->get_resolution();

        // create in flight fence
        m_flight_fences.resize(m_num_flights);
        for (int i = 0; i < m_num_flights; i++)
        {
            m_flight_fences[i] = Gp::Fence(m_device, "flight_fence" + std::to_string(i));
        }

        // create command pool and descriptor pool
        m_graphics_command_pool.resize(m_num_flights);
        m_descriptor_pools.resize(m_num_flights);
        m_image_ready_semaphores.resize(m_num_flights);
        m_image_presentable_semaphores.resize(m_num_flights);
        for (size_t i = 0; i < m_num_flights; i++)
        {
            m_graphics_command_pool[i]        = Gp::CommandPool(m_device);
            m_descriptor_pools[i]             = Gp::DescriptorPool(m_device);
            m_image_ready_semaphores[i]       = Gp::Semaphore(m_device);
            m_image_presentable_semaphores[i] = Gp::Semaphore(m_device);
        }

        // create swapchain
        m_swapchain        = Gp::Swapchain(m_device, *m_window, "main_swapchain");
        m_swapchain_length = m_swapchain.m_num_images;
        m_swapchain_textures.resize(m_swapchain_length);
        for (size_t i = 0; i < m_swapchain.m_num_images; i++)
        {
            m_swapchain_textures[i] = Gp::Texture(m_device, m_swapchain, i);
        }

        // staging buffer manager
        m_staging_buffer_manager = Gp::StagingBufferManager(m_device, "main_staging_buffer_manager");
        m_asset_manager          = AssetManager(m_device, &m_staging_buffer_manager);

        // initialize renderer
        m_renderer.init(m_device, m_window->get_resolution(), m_swapchain_textures);

        // initalize camera
        m_camera = FpsCamera(float3(10.0f, 10.0f, 10.0f),
                             float3(0, 0, 0),
                             float3(0, 1, 0),
                             radians(60.0f),
                             float(resolution.x) / float(resolution.y));

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
    }

    void
    loop(const size_t i_flight)
    {
        GlfwHandler::Inst().poll_events();
        m_camera.update(m_window, m_window->m_stop_watch.m_average_frame_time * 0.01f);

        // wait for resource in this flight to be ready
        m_flight_fences[i_flight].wait();

        // reset all resource
        m_flight_fences[i_flight].reset();
        m_graphics_command_pool[i_flight].reset();
        m_descriptor_pools[i_flight].reset();

        // update image index
        m_swapchain.update_image_index(&m_image_ready_semaphores[i_flight]);

        // setup context
        RenderContext ctx;
        ctx.m_device                       = m_device;
        ctx.m_graphics_command_pool        = &m_graphics_command_pool[i_flight];
        ctx.m_descriptor_pool              = &m_descriptor_pools[i_flight];
        ctx.m_image_ready_semaphore        = &m_image_ready_semaphores[i_flight];
        ctx.m_image_presentable_semaphores = &m_image_presentable_semaphores[i_flight];
        ctx.m_flight_fence                 = &m_flight_fences[i_flight];
        ctx.m_flight_index                 = i_flight;
        ctx.m_image_index                  = m_swapchain.m_image_index;
        ctx.m_swapchain_texture            = &m_swapchain_textures[m_swapchain.m_image_index];
        ctx.m_staging_buffer_manager       = &m_staging_buffer_manager;
        ctx.m_dummy_buffer                 = &m_dummy_buffer;
        ctx.m_dummy_texture                = &m_dummy_texture;

        RenderParams render_params;
        render_params.m_vertex_buffers = &m_asset_manager.m_vertex_buffers;
        render_params.m_index_buffers  = &m_asset_manager.m_index_buffers;
        render_params.m_materials  = &m_asset_manager.m_pbr_materials;
        render_params.m_textures   = &m_asset_manager.m_textures;
        render_params.m_resolution = m_window->get_resolution();
        render_params.m_static_meshes =
            m_asset_manager.m_pbr_objects; // TODO:: for now, we render all meshes as static
        render_params.m_is_static_mesh_dirty = false;
        render_params.m_fps_camera           = &m_camera;
        render_params.m_is_shaders_dirty     = false;

        if (glfwGetKey(m_window->m_glfw_window, GLFW_KEY_R) == GLFW_PRESS)
        {
            Logger::Info("marked shaders dirty");
            render_params.m_is_shaders_dirty = true;
        }

        // run renderer
        m_renderer.loop(ctx, render_params);

        m_swapchain.present(&m_image_presentable_semaphores[i_flight]);
        m_window->update();
    }

    void
    run()
    {
        size_t i_flight = 0;

        int2 sponza_mesh = m_asset_manager.add_pbr_object("sponza/sponza.obj");
        //int2 box_mesh = m_asset_manager.add_pbr_object("cube.obj");

        while (!m_window->should_close_window())
        {
            loop(i_flight);
            i_flight = (i_flight + 1) % m_num_flights;
        }
    }
};