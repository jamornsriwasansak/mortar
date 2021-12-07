#pragma once

#include <filesystem>
#include <imgui.h>

struct AssetBrowserQuick
{
    AssetBrowserQuick() {}

    struct Path
    {
        std::filesystem::path m_filepath         = "";
        int                   m_similarity_score = -1;
    };

    std::vector<Path> m_filepaths;

    void
    init()
    {
    }

    int
    longest_common_subsequence(const std::string & text1, const std::string & text2, std::vector<int> * dptable)
    {
        int       result         = 0;
        const int dptable_width  = text2.length() + 1;
        const int dptable_height = text1.length() + 1;
        dptable->resize(dptable_width * dptable_height);
        for (int i = 0; i <= text1.length(); i++)
        {
            for (int j = 0; j <= text2.length(); j++)
            {
                if (i == 0 || j == 0)
                {
                    dptable->at(dptable_width * i + j) = 0;
                }
                else if (text1[i - 1] == text2[j - 1])
                {
                    const int val     = dptable->at(dptable_width * (i - 1) + (j - 1));
                    const int updated = val + 1;
                    dptable->at(dptable_width * i + j) = updated;
                    result                             = std::max(result, updated);
                }
                else
                {
                    const int val0                     = dptable->at(dptable_width * (i - 1) + j);
                    const int val1                     = dptable->at(dptable_width * i + (j - 1));
                    const int updated                  = max(val0, val1);
                    dptable->at(dptable_width * i + j) = updated;
                    result                             = std::max(result, updated);
                }
            }
        }
        return result;
    }

    void
    update_filepaths(const std::filesystem::path & filepath)
    {
        using namespace std::filesystem;
        directory_iterator iter_end;
        for (directory_iterator iter(filepath); iter != iter_end; iter++)
        {
            const std::filesystem::path path      = iter->path();
            const size_t                file_size = iter->file_size();
            // set node type
            if (is_directory(iter->status()))
            {
                update_filepaths(path);
            }
            else
            {
                m_filepaths.push_back({ path, -1 });
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

            // assign filepath score if input is not empty
            const std::string search_text = y;
            std::vector<int>  dptable;
            for (auto & filepath : m_filepaths)
            {
                filepath.m_similarity_score =
                    longest_common_subsequence(search_text, filepath.m_filepath.filename().string(), &dptable);
            }

            // sort files by score
            std::sort(m_filepaths.begin(), m_filepaths.end(), [](const Path & f1, const Path & f2) {
                return (f1.m_similarity_score > f2.m_similarity_score);
            });

            if (ImGui::BeginTable("split2", 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_NoSavedSettings))
            {
                for (const auto & filepath : m_filepaths)
                {
                    const std::string m_filename      = filepath.m_filepath.filename().string();
                    const std::string m_full_filepath = filepath.m_filepath.string();
                    const float file_size_in_bytes =
                        static_cast<float>(std::filesystem::file_size(filepath.m_filepath));
                    const std::string m_file_size     = std::to_string(file_size_in_bytes / 1024.0f / 1024.0f);

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    if (ImGui::Selectable(m_filename.c_str(), selected == i, ImGuiSelectableFlags_SpanAllColumns))
                    {
                        selected = i;
                    }
                    ImGui::TableNextColumn();
                    ImGui::Text(m_full_filepath.c_str());
                    ImGui::TableNextColumn();
                    ImGui::Text(m_file_size.c_str());
                    i++;
                }
                ImGui::EndTable();
            }
        }
        ImGui::End();
    }
};