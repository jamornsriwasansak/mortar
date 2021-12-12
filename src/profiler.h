#pragma once

#include "rhi/rhi.h"

struct ProfilingInterval
{
    std::string m_name;
    uint32_t    m_begin_query_index;
    uint32_t    m_end_query_index;
    uint64_t    m_begin_time = std::numeric_limits<uint64_t>::max();
    uint64_t    m_end_time   = std::numeric_limits<uint64_t>::max();

    ProfilingInterval(const std::string & name, const uint32_t begin_query_index, const uint32_t end_query_index)
    : m_name(name), m_begin_query_index(begin_query_index), m_end_query_index(end_query_index)
    {
    }
};

struct Profiler
{
    Rhi::QueryPool                 m_query_pool;
    uint32_t                       m_query_counter       = 0;
    std::vector<ProfilingInterval> m_profiling_intervals = {};

    Profiler(const std::string & name, const Rhi::Device & device, const uint32_t num_max_markers)
    : m_query_pool(name + "_query_pool", device, Rhi::QueryType::Timestamp, num_max_markers)
    {
    }

    std::pair<uint32_t, uint32_t>
    get_new_profiling_interval_query_indices(const std::string & name)
    {
        // Get new query index
        const uint32_t begin_query_index = m_query_counter;
        const uint32_t end_query_index   = begin_query_index + 1;
        m_query_counter += 2;

        // Return intervals
        m_profiling_intervals.emplace_back(name, begin_query_index, end_query_index);

        return std::make_pair(begin_query_index, end_query_index);
    }

    void
    reset()
    {
        m_query_counter = 0;
        m_profiling_intervals.clear();
    }

    void
    summarize()
    {
        std::vector<uint64_t> timestamps = m_query_pool.get_query_result(m_query_counter);
        for (ProfilingInterval & profiling_interval : m_profiling_intervals)
        {
            profiling_interval.m_begin_time = timestamps[profiling_interval.m_begin_query_index];
            profiling_interval.m_end_time   = timestamps[profiling_interval.m_end_query_index];
        }
    }
};
