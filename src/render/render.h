#pragma once

#include "graphicsapi/graphicsapi.h"
#include "integrators/flat/flat.h"
#include "rendercontext.h"

struct Render
{
    Gp::Entry                       m_entry;
    Gp::Device                      m_device;
    Gp::PhysicalDevice              m_physical_device;
    Gp::Swapchain                   m_swapchain;
    std::vector<Gp::Texture>        m_swapchain_color_attachments;
    std::vector<Gp::Fence>          m_flight_fences;
    std::vector<Gp::CommandPool>    m_graphics_command_pool;
    std::vector<Gp::DescriptorPool> m_descriptor_pools;
    std::vector<Gp::Semaphore>      m_image_ready_semaphores;
    std::vector<Gp::Semaphore>      m_image_presentable_semaphores;
    Window                          m_main_window;
    int2                            m_resolution;
    size_t                          m_swapchain_length;
    size_t                          m_num_flights;

    Render() {}

    void
    init()
    {
        m_resolution  = int2(1920, 1080);
        m_num_flights = 2;

        // create main window
        m_main_window = Window("Mortar", m_resolution);

        // create entry
        const bool is_debug = true;
        m_entry             = Gp::Entry(m_main_window, is_debug);
        m_physical_device   = m_entry.get_graphics_devices()[0];
        m_device            = Gp::Device(m_physical_device);

        // create in flight fence
        m_flight_fences.resize(m_num_flights);
        for (int i = 0; i < m_num_flights; i++)
        {
            m_flight_fences[i] = Gp::Fence(&m_device, "flight_fence" + std::to_string(i));
        }

        // create command pool and descriptor pool
        m_graphics_command_pool.resize(m_num_flights);
        m_descriptor_pools.resize(m_num_flights);
        m_image_ready_semaphores.resize(m_num_flights);
        m_image_presentable_semaphores.resize(m_num_flights);
        for (size_t i = 0; i < m_num_flights; i++)
        {
            m_graphics_command_pool[i]        = Gp::CommandPool(&m_device);
            m_descriptor_pools[i]             = Gp::DescriptorPool(&m_device);
            m_image_ready_semaphores[i]       = Gp::Semaphore(&m_device);
            m_image_presentable_semaphores[i] = Gp::Semaphore(&m_device);
        }


        // create swapchain
        m_swapchain        = Gp::Swapchain(&m_device, m_main_window, "main_swapchain");
        m_swapchain_length = m_swapchain.m_num_images;
        m_swapchain_color_attachments.resize(m_swapchain_length);
        for (size_t i = 0; i < m_swapchain.m_num_images; i++)
        {
            m_swapchain_color_attachments[i] = Gp::Texture(&m_device, m_swapchain, i);
        }
    }

    void
    loop(const size_t i_flight)
    {
        // setup context
        RenderContext ctx;
        ctx.m_device                       = &m_device;
        ctx.m_graphics_command_pool        = &m_graphics_command_pool[i_flight];
        ctx.m_descriptor_pool              = &m_descriptor_pools[i_flight];
        ctx.m_image_ready_semaphore        = &m_image_ready_semaphores[i_flight];
        ctx.m_image_presentable_semaphores = &m_image_presentable_semaphores[i_flight];

        // wait for resource in this flight to be ready
        m_flight_fences[i_flight].wait();

        // reset all resource
        m_flight_fences[i_flight].reset();
        m_graphics_command_pool[i_flight].reset();
        m_descriptor_pools[i_flight].reset();

        // update image index
        m_swapchain.update_image_index(ctx.m_image_ready_semaphore);

        m_swapchain.present(&m_image_presentable_semaphores[i_flight]);
        m_main_window.update();
        GlfwHandler::Inst().poll_events();
    }

    void
    run()
    {
        size_t i_flight = 0;

        while (!m_main_window.should_close_window())
        {
            loop(i_flight);
            i_flight = (i_flight + 1) % m_num_flights;
        }
    }
};