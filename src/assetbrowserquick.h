#pragma once

#include <filesystem>
#include <imgui.h>

struct AssetBrowserQuick
{
    AssetBrowserQuick() {}

    std::vector<std::filesystem::path> m_filepaths;

    void
    init()
    {
    }

    void
    update_filepaths(const std::filesystem::path & filepath)
    {
        using namespace std::filesystem;

        directory_iterator iter_end;
        for (directory_iterator iter(filepath); iter != iter_end; iter++)
        {
            const std::filesystem::path path      = iter->path();
            const std::string           name      = iter->path().filename().string();
            const size_t                file_size = iter->file_size();

            // set node type
            if (is_directory(iter->status()))
            {
                update_filepaths(path);
            }
            else
            {
                m_filepaths.push_back(filepath);
            }
        }
    }

    bool x      = true;
    bool p_open = true;

    char y[100]   = "";
    int  selected = -1;

    void
    loop()
    {
        ImGui::ShowDemoWindow(&x);

        static int       corner       = 0;
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
                                        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                                        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;

        if (corner != -1)
        {
            const float           PAD      = 10.0f;
            const ImGuiViewport * viewport = ImGui::GetMainViewport();
            ImVec2 work_pos = viewport->WorkPos; // Use work area to avoid menu-bar/task-bar, if any!
            ImVec2 work_size = viewport->WorkSize;
            ImVec2 window_pos, window_pos_pivot;
            window_pos.x       = work_size.x / 2.0f;
            window_pos.y       = work_size.y * 1.0f / 4.0f;
            window_pos_pivot.x = 0.5f;
            window_pos_pivot.y = 0.0f;
            ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
            ImGui::SetNextWindowViewport(viewport->ID);
            window_flags |= ImGuiWindowFlags_NoMove;
        }

        ImGui::SetNextWindowBgAlpha(0.35f); // Transparent background

        int i = 0;

        if (ImGui::Begin("Quick Open Dialog", &p_open, window_flags))
        {
            ImGui::InputText("", y, 100);

            m_filepaths.clear();
            update_filepaths(".");

            if (ImGui::BeginTable("split2", 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_NoSavedSettings))
            {
                for (const auto & filepath : m_filepaths)
                {
                    std::string m_filename = filepath.filename().string();
                    std::string m_full_filepath = filepath.string();

                    if (i < 10)
                    {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        if (ImGui::Selectable(m_filename.c_str(), selected == i, ImGuiSelectableFlags_SpanAllColumns))
                        {
                            selected = i;
                        }
                        ImGui::TableNextColumn();
                        ImGui::Text(m_full_filepath.c_str());
                        ImGui::TableNextColumn();
                        ImGui::Text("n/a");
                    }
                    else if (i == 10)
                    {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::Text(".....");
                    }
                    i++;
                }
                ImGui::EndTable();
            }
        }
        ImGui::End();
    }
};