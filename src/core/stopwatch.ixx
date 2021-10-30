#pragma once

#include <chrono>
#include <ctime>

struct StopWatch
{
    std::chrono::high_resolution_clock::time_point m_start_time = std::chrono::high_resolution_clock::now();

    StopWatch() {}

    inline void
    reset()
    {
        m_start_time = std::chrono::high_resolution_clock::now();
    }

    inline long long
    time_milli_sec()
    {
        std::chrono::high_resolution_clock::time_point endTime = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(endTime - m_start_time).count();
    }

    inline long long
    time_micro_sec()
    {
        std::chrono::high_resolution_clock::time_point endTime = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(endTime - m_start_time).count();
    }
};

struct AvgFrameTimeStopWatch
{
    StopWatch m_stop_watch         = StopWatch();
    float     m_average_frame_time = 0.0f;
    float     m_sum_time           = 0.0f;
    float     m_time_threshold     = 1000.0f;
    uint16_t  m_sum_tick           = 0;
    uint16_t  m_tick_threshold     = 10000;
    bool      m_just_updated       = false;

    AvgFrameTimeStopWatch() {}

    void
    reset()
    {
        m_stop_watch.reset();
    }

    void
    tick()
    {
        long long   time_micro_sec = m_stop_watch.time_micro_sec();
        const float frame_time     = static_cast<float>(time_micro_sec) / 1000.0f;
        m_stop_watch.reset();

        // update sum
        m_sum_time     = m_sum_time + frame_time;
        m_sum_tick     = m_sum_tick + 1;
        m_just_updated = false;

        // update average_time if sum_time or sum_tick is beyond the threshold
        if (m_sum_time >= m_time_threshold || m_sum_tick >= m_tick_threshold)
        {
            m_average_frame_time = m_sum_time / static_cast<float>(m_sum_tick);
            m_sum_time           = 0.0f;
            m_sum_tick           = 0;
            m_just_updated       = true;
        }
    }
};