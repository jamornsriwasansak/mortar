#pragma once

#include "pch/pch.h"

struct GuiEventCoordinator
{
    bool m_use_all_debug_gui               = true;
    bool m_display_raytrace_visualize_menu = true;
    bool m_display_main_pipeline_mode      = true;
    bool m_display_profiler_gui            = true;
    bool m_show_profiler_gui               = true;

    bool
    is_gui_being_used() const
    {
        const bool is_active =
            ImGui::IsAnyItemActive() || ImGui::IsAnyItemFocused() || ImGui::IsAnyItemHovered();
        return is_active;
    }

    void
    draw_main_dockspace() const
    {
        constexpr ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;

        bool p_open = true;

        constexpr ImGuiWindowFlags window_flags =
            ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;
        const ImGuiViewport * viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("DockSpace Demo", &p_open, window_flags);
        ImGui::PopStyleVar();
        ImGui::PopStyleVar(2);

        // Submit the DockSpace
        ImGuiIO & io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
        {
            ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
        }

        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("Options"))
            {
                // Disabling fullscreen would allow the window to be moved to the front of other
                // windows, which we can't undo at the moment without finer window depth/z control.
                bool opt_fullscreen;
                bool opt_padding;
                ImGui::MenuItem("Fullscreen", NULL, &opt_fullscreen);
                ImGui::MenuItem("Padding", NULL, &opt_padding);
                ImGui::Separator();

                if (ImGui::MenuItem("Flag: NoSplit", "", (dockspace_flags & ImGuiDockNodeFlags_NoSplit) != 0))
                {
                }
                if (ImGui::MenuItem("Flag: NoResize", "", (dockspace_flags & ImGuiDockNodeFlags_NoResize) != 0))
                {
                }
                if (ImGui::MenuItem("Flag: NoDockingInCentralNode",
                                    "",
                                    (dockspace_flags & ImGuiDockNodeFlags_NoDockingInCentralNode) != 0))
                {
                }
                if (ImGui::MenuItem("Flag: AutoHideTabBar", "", (dockspace_flags & ImGuiDockNodeFlags_AutoHideTabBar) != 0))
                {
                }
                if (ImGui::MenuItem("Flag: PassthruCentralNode",
                                    "",
                                    (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode) != 0,
                                    opt_fullscreen))
                {
                }
                ImGui::Separator();
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }
        ImGui::End();
    }

    GuiEventCoordinator() {}

    GuiEventCoordinator(const GuiEventCoordinator &) = delete;

    void
    operator=(const GuiEventCoordinator & other) = delete;
};