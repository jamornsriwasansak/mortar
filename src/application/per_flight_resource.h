#pragma once

#include "pch/pch.h"

#include "rhi/rhi.h"

struct PerFlightResource
{
    Rhi::Fence          m_flight_fence;
    Rhi::CommandPool    m_graphics_command_pool;
    Rhi::CommandPool    m_compute_command_pool;
    Rhi::CommandPool    m_transfer_command_pool;
    Rhi::DescriptorPool m_descriptor_pool;
    Rhi::Semaphore      m_image_ready_semaphore;
    Rhi::Semaphore      m_image_presentable_semaphore;
    Rhi::QueryPool      m_timestamp_query_pool;

    std::chrono::high_resolution_clock::time_point m_host_reset_time;

    static constexpr uint32_t num_descriptors = 1000;
    static constexpr uint32_t num_queries     = 1000;

    PerFlightResource(const std::string & name, const Rhi::Device & device)
    : m_flight_fence(name + "_flight_fence", device),
      m_graphics_command_pool(name + "_graphics_command_pool", device, Rhi::QueueType::Graphics),
      m_compute_command_pool(name + "_graphics_command_pool", device, Rhi::QueueType::Compute),
      m_transfer_command_pool(name + "_graphics_command_pool", device, Rhi::QueueType::Transfer),
      m_descriptor_pool(name + "_descriptor_pool", device, num_descriptors),
      m_image_ready_semaphore(name + "_image_read_semaphore", device),
      m_image_presentable_semaphore(name + "_image_presentable_semaphore", device),
      m_timestamp_query_pool(name + "_timestamp_query_pool", device, Rhi::QueryType::Timestamp, num_queries)
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
        m_host_reset_time = std::chrono::high_resolution_clock::now();
        m_flight_fence.reset();
        m_graphics_command_pool.reset();
        m_compute_command_pool.reset();
        m_transfer_command_pool.reset();
        m_descriptor_pool.reset();
    }
};
