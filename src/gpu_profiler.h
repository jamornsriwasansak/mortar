#pragma once

#include "rhi/rhi.h"

#include "core/gui_event_coordinator.h"

struct QueryIndices
{
    uint32_t m_begin_index;
    uint32_t m_end_index;
};

struct GpuProfilingInterval
{
    std::string m_name;
    uint32_t    m_begin_query_index;
    uint32_t    m_end_query_index;
    uint64_t    m_begin_timestamp  = std::numeric_limits<uint64_t>::max();
    uint64_t    m_end_timestamp    = std::numeric_limits<uint64_t>::max();
    int         m_num_scope_layers = 0;

    GpuProfilingInterval() {}

    GpuProfilingInterval(const std::string_view & name,
                         const uint32_t           begin_query_index,
                         const uint32_t           end_query_index,
                         const int                num_scope_layers)
    : m_name(name),
      m_begin_query_index(begin_query_index),
      m_end_query_index(end_query_index),
      m_num_scope_layers(num_scope_layers)
    {
    }
};

struct GpuProfiler
{
    Rhi::QueryPool                    m_query_pool;
    uint32_t                          m_query_counter = 0;
    std::vector<GpuProfilingInterval> m_profiling_intervals;
    std::string                       m_name;
    float                             m_ns_from_timestamp;
    int                               m_num_scope_layers = 0;

    GpuProfiler(const std::string & name, const Rhi::Device & device, const uint32_t num_max_markers)
    : m_name(name),
      m_query_pool(name + "_query_pool", device, Rhi::QueryType::Timestamp, num_max_markers),
      m_ns_from_timestamp(device.get_timestamp_period())
    {
    }

    QueryIndices
    get_new_profiling_interval_query_indices(const std::string_view & name)
    {
        // Get new query index
        const uint32_t begin_query_index = m_query_counter;
        const uint32_t end_query_index   = begin_query_index + 1;
        m_query_counter += 2;

        // Return intervals
        m_profiling_intervals.emplace_back(name, begin_query_index, end_query_index, m_num_scope_layers);

        return QueryIndices{ begin_query_index, end_query_index };
    }

    void
    reset()
    {
        assert(m_num_scope_layers == 0);
        m_query_pool.reset();
        m_query_counter = 0;
        m_profiling_intervals.clear();
    }

    void
    summarize()
    {
        std::vector<uint64_t> timestamps = m_query_pool.get_query_result(m_query_counter);
        for (GpuProfilingInterval & profiling_interval : m_profiling_intervals)
        {
            profiling_interval.m_begin_timestamp = timestamps[profiling_interval.m_begin_query_index];
            profiling_interval.m_end_timestamp   = timestamps[profiling_interval.m_end_query_index];
        }
    }
};

struct GpuProfilingScope
{
    Rhi::CommandBuffer & m_command_buffer;
    GpuProfiler *        m_gpu_profiler;
    QueryIndices         m_query_indices = {};

    GpuProfilingScope(const std::string_view & name, Rhi::CommandBuffer & cmd_buffer, GpuProfiler * gpu_profiler)
    : m_command_buffer(cmd_buffer), m_gpu_profiler(gpu_profiler)
    {
        if (m_gpu_profiler)
        {
            m_query_indices = m_gpu_profiler->get_new_profiling_interval_query_indices(name);
            m_gpu_profiler->m_num_scope_layers += 1;
            m_command_buffer.write_timestamp(m_gpu_profiler->m_query_pool, m_query_indices.m_begin_index);
        }
    }

    ~GpuProfilingScope()
    {
        if (m_gpu_profiler)
        {
            m_gpu_profiler->m_num_scope_layers -= 1;
            m_command_buffer.write_timestamp(m_gpu_profiler->m_query_pool, m_query_indices.m_end_index);
        }
    }
};

struct GpuProfilerGui
{
    std::vector<std::vector<GpuProfilingInterval>> m_profiling_intervals_of_flights = {};
    float                                          m_zoom_level                     = 1.0f;
    bool                                           m_pause                          = false;
    float                                          m_ns_from_timestamp              = 1.0f;
    size_t                                         m_flight_counter                 = 0;

    GpuProfilerGui(const size_t num_flights_to_records)
    : m_profiling_intervals_of_flights(num_flights_to_records)
    {
    }

    void
    update(const std::vector<GpuProfilingInterval> & profiling_intervals, const float ns_from_timestamp)
    {
        if (!m_pause)
        {
            m_profiling_intervals_of_flights[m_flight_counter % m_profiling_intervals_of_flights.size()] =
                profiling_intervals;
            m_ns_from_timestamp = ns_from_timestamp;
            m_flight_counter    = (m_flight_counter + 1) % m_profiling_intervals_of_flights.size();
        }
    }

    void
    draw_gui(GuiEventCoordinator & gui_event_coordinator)
    {
        if (ImGui::Begin("Gpu Profiling", &gui_event_coordinator.m_display_profiler_gui))
        {
            // 60 fps = 16.66 ms = 16.66 * 10^6 ns
            constexpr float  ms_from_ns                = 1e-6f;
            constexpr float  expected_frame_time_in_ns = 16.66f * 1e6f;
            constexpr float  graph_scale               = 1000.0f;
            constexpr float2 padding                   = float2(5.0f, 25.0f);

            const float width_from_timestamp =
                graph_scale * m_ns_from_timestamp / expected_frame_time_in_ns * m_zoom_level;
            const float button_height = ImGui::GetFontSize() * 1.5f;

            ImGui::Checkbox("Pause", &m_pause);
            ImGui::SliderFloat("Zoom", &m_zoom_level, 0.0f, 10.0f, "%f", 1.0f);

            size_t   i_flight   = m_flight_counter;
            uint64_t begin_time = 0;
            if (m_profiling_intervals_of_flights[m_flight_counter].size() > 0)
            {
                begin_time = m_profiling_intervals_of_flights[m_flight_counter][0].m_begin_timestamp;
                // Draw all bars
                do
                {
                    const std::vector<GpuProfilingInterval> & profiling_intervals =
                        m_profiling_intervals_of_flights[i_flight];

                    ImGuiWindowFlags window_flags = ImGuiWindowFlags_HorizontalScrollbar |
                                                    ImGuiWindowFlags_AlwaysHorizontalScrollbar;
                    // Style the button
                    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
                    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.0f);

                    if (ImGui::BeginChild("ChildL",
                                          ImVec2(ImGui::GetWindowContentRegionWidth(), button_height * 8),
                                          true,
                                          window_flags))
                    {
                        // Color the button
                        ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0 / 7.0f, 0.6f, 0.6f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                                              (ImVec4)ImColor::HSV(0 / 7.0f, 0.7f, 0.7f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(0 / 7.0f, 0.8f, 0.8f));

                        // Populate buttons (represent interval period)
                        for (const GpuProfilingInterval & profiling_interval : profiling_intervals)
                        {
                            // Compute button corner
                            const float corner_x =
                                (profiling_interval.m_begin_timestamp - begin_time) * width_from_timestamp;
                            const float corner_y = profiling_interval.m_num_scope_layers * button_height;

                            // Compute button Width
                            float button_width = static_cast<float>(profiling_interval.m_end_timestamp -
                                                                    profiling_interval.m_begin_timestamp) *
                                                 width_from_timestamp;

                            // Work around when button_width is too long and breaks things
                            // or when button_width is infinitely short and does not display anything
                            if (button_width > 100000.0f || button_width == 0.0f)
                                button_width = 0.0000001f;

                            // Draw button (profiling range)
                            ImGui::SetCursorPos(ImVec2(corner_x + padding.x, corner_y + padding.y));
                            ImGui::Button(profiling_interval.m_name.c_str(), ImVec2(button_width, button_height));

                            // Show more information if hovered
                            if (ImGui::IsItemHovered())
                            {
                                ImGui::SetTooltip("%s\ntime spent: %.4fms",
                                                  profiling_interval.m_name.c_str(),
                                                  static_cast<float>(profiling_interval.m_end_timestamp -
                                                                     profiling_interval.m_begin_timestamp) *
                                                      m_ns_from_timestamp * ms_from_ns);
                            }
                        }

                        // Pop colors and styles
                        ImGui::PopStyleColor(3);
                        ImGui::EndChild();
                    }
                    ImGui::PopStyleVar(2);
                    i_flight = (i_flight + 1) % m_profiling_intervals_of_flights.size();
                } while (i_flight != m_flight_counter);

                // Draw Texts
                do
                {
                    const std::vector<GpuProfilingInterval> & profiling_intervals =
                        m_profiling_intervals_of_flights[i_flight];

                    char             buffer[128];
                    constexpr size_t tab_size = 4;
                    for (const GpuProfilingInterval & profiling_interval : profiling_intervals)
                    {
                        for (size_t i = 0; i < profiling_interval.m_num_scope_layers; i++)
                        {
                            for (size_t j = 0; j < tab_size; j++)
                            {
                                buffer[i * tab_size + j] = ' ';
                            }
                        }
                        buffer[(profiling_interval.m_num_scope_layers) * tab_size] = 0;
                        ImGui::Text("%s%s: %.4fms",
                                    buffer,
                                    profiling_interval.m_name.c_str(),
                                    static_cast<float>(profiling_interval.m_end_timestamp -
                                                       profiling_interval.m_begin_timestamp) *
                                        m_ns_from_timestamp * ms_from_ns);
                    }
                    ImGui::Text("");
                    i_flight = (i_flight + 1) % m_profiling_intervals_of_flights.size();
                } while (i_flight != m_flight_counter);
            }
        }
        ImGui::End();
    }
};
