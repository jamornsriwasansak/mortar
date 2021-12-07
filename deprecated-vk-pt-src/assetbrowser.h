#pragma once

#include <filesystem>
#include <imgui.h>

struct AssetBrowser
{
    AssetBrowser() {}

    void
    init()
    {
    }

    bool x        = true;
    int  selected = 0;

    enum class FileNodeType
    {
        Unknown,
        UnknownFileType,
        Folder,
        Root
    };

    struct FileTreeNode
    {
        FileNodeType          m_node_type         = FileNodeType::Unknown;
        std::filesystem::path m_path              = "";
        std::string           m_name              = ":null";
        size_t                m_size              = 0;
        size_t                m_child_index_begin = 0;
        size_t                m_child_index_end   = 0;

        FileTreeNode() {}

        FileTreeNode(const FileNodeType node_type, const std::filesystem::path & path, const std::string & name, const size_t size)
        : m_node_type(node_type), m_path(path), m_name(name), m_size(size), m_child_index_begin(0), m_child_index_end(0)
        {
        }
    };

    std::vector<FileTreeNode> m_file_tree_nodes;

    void
    update_file_node(const size_t parent_index)
    {
        using namespace std::filesystem;

        const path         path              = m_file_tree_nodes[parent_index].m_path;
        const size_t       child_index_begin = m_file_tree_nodes.size();
        directory_iterator iter_end;
        for (directory_iterator iter(path); iter != iter_end; iter++)
        {
            const std::filesystem::path path      = iter->path();
            const std::string           name      = iter->path().filename().string();
            const size_t                file_size = iter->file_size();

            // set node type
            FileNodeType node_type;
            if (is_directory(iter->status()))
            {
                node_type = FileNodeType::Folder;
            }
            else
            {
                node_type = FileNodeType::UnknownFileType;
            }

            m_file_tree_nodes.emplace_back(node_type, path, name, file_size);
        }
        const size_t child_index_end = m_file_tree_nodes.size();

        for (size_t i = child_index_begin; i < child_index_end; i++)
        {
            if (m_file_tree_nodes[i].m_node_type == FileNodeType::Folder)
            {
                update_file_node(i);
            }
        }
        m_file_tree_nodes[parent_index].m_child_index_begin = child_index_begin;
        m_file_tree_nodes[parent_index].m_child_index_end   = child_index_end;
    }

    void
    display_ui(const FileTreeNode & node)
    {
        for (size_t i = node.m_child_index_begin; i < node.m_child_index_end; i++)
        {
            auto & child_node = m_file_tree_nodes[i];
                ImGui::TableNextRow();
                if (child_node.m_node_type == FileNodeType::Folder)
                {
                    ImGui::TableNextColumn();
                    bool open = ImGui::TreeNodeEx(child_node.m_name.c_str(), ImGuiTreeNodeFlags_SpanFullWidth);
                    ImGui::TableNextColumn();
                    ImGui::TextDisabled("--");
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted("folder");
                    if (open)
                    {
                        display_ui(child_node);
                        ImGui::TreePop();
                    }
                }
                else
                {
                    ImGui::TableNextColumn();
                    ImGui::TreeNodeEx(child_node.m_name.c_str(),
                                      ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet |
                                          ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanFullWidth);
                    ImGui::TableNextColumn();
                    ImGui::Text("%d", node.m_size);
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted("file");
                }
        }
    }

    bool is_tree_dirty = true;

    void
    loop()
    {
        if (is_tree_dirty)
        {
            m_file_tree_nodes.resize(0);
            FileTreeNode root(FileNodeType::Root, ".", ".", 0);
            m_file_tree_nodes.push_back(root);
            update_file_node(0);

            //is_tree_dirty = false;
        }

        ImGui::ShowDemoWindow(&x);

        if (ImGui::Begin("Asset Browser", &x))
        {
            static ImGuiTableFlags flags = ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH |
                                           ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg |
                                           ImGuiTableFlags_NoBordersInBody;

            if (ImGui::BeginTable("3ways", 3, flags))
            {
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoHide);
                ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableHeadersRow();
                display_ui(m_file_tree_nodes[0]);
                ImGui::EndTable();
            }
            ImGui::End();
        }
    }
};