#pragma once

#include <chrono>
#include <map>
#include <mutex>
#include <ostream>
#include <thread>
#include <vector>

struct Trace
{
    std::map<std::thread::id, std::vector<std::string>>                                    m_name_stack;
    std::map<std::thread::id, std::vector<std::chrono::high_resolution_clock::time_point>> m_time_stack;
    std::ostream *                                                                         m_stream = nullptr;
    std::mutex                                                                             m_pop_mutex;
    bool                                                                                   m_need_comma = false;

#define QUOTE(v) "\"" << v << "\""

    static Trace &
    Get()
    {
        static Trace singleton;
        return singleton;
    }

    void
    begin(std::ostream * stream)
    {
        m_stream = stream;
        *m_stream << "{\"traceEvents\":[\n";
    }

    void
    push(const std::string & name)
    {
        const std::thread::id thread_id  = std::this_thread::get_id();
        const auto            start_time = std::chrono::high_resolution_clock::now();
        m_time_stack[thread_id].push_back(start_time);
        m_name_stack[thread_id].push_back(name);
    }

    void
    pop()
    {
        const std::thread::id thread_id = std::this_thread::get_id();

        // get start time and end time
        const auto start_time = m_time_stack[thread_id].back();
        const auto end_time   = std::chrono::high_resolution_clock::now();

        // get duration and start time in milliseconds
        const auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
        const auto start_ms = std::chrono::time_point_cast<std::chrono::microseconds>(start_time).time_since_epoch().count();

        // get name
        const std::string & name = m_name_stack[thread_id].back();

        std::lock_guard<std::mutex> guard(m_pop_mutex);
        {
            if (m_need_comma) *m_stream << ",";
            *m_stream << "{" << QUOTE("dur") << ":" << duration << "," << QUOTE("name") << ":" << QUOTE(name) << ","
                      << QUOTE("ph") << ":" << QUOTE("X") << "," << QUOTE("pid") << ":" << 0 << "," << QUOTE("tid")
                      << ":" << thread_id << "," << QUOTE("ts") << ":" << start_ms << "}";
            m_need_comma = true;
        }

        m_time_stack[thread_id].pop_back();
        m_name_stack[thread_id].pop_back();
    }

    void
    end()
    {
        *m_stream << "]}";
    }

private:
    Trace() {}

    Trace(const Trace &) = delete;

    Trace(Trace &&) = delete;

    Trace &
    operator=(const Trace &) = delete;

    Trace &
    operator=(Trace &&) = delete;
};