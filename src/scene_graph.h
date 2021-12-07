#pragma once

#include "pch/pch.h"
#include "rhi/rhi.h"

struct SceneGraphGeometry
{
    uint32_t         m_vbuf_offset       = 0;
    uint32_t         m_ibuf_offset       = 0;
    size_t           m_num_vertices      = 0;
    size_t           m_num_indices       = 0;
    uint32_t         m_material_id       = 0;
    bool             m_is_static         = true;
    StandardMaterial m_standard_material = {};
};

struct SceneGraphLeaf
{
    bool     m_is_instance = false;
    uint32_t m_instance_id = std::numeric_limits<uint32_t>::max();
    uint32_t m_geometry_id = std::numeric_limits<uint32_t>::max();
    // multiplication of tranforms from root to leaf
    float4x4 m_total_transform;
};

struct SceneGraphNode
{
    SceneGraphNode * m_parent                                        = nullptr;
    float4x4         m_transform                                     = glm::identity<float4x4>();
    bool             m_is_dirty                                      = true;
    std::variant<SceneGraphLeaf, std::vector<SceneGraphNode>> m_info = {};

    SceneGraphNode(const bool as_leaf)
    {
        if (as_leaf)
        {
            m_info = SceneGraphLeaf();
        }
        else
        {
            m_info = std::vector<SceneGraphNode>();
        }
    }

    SceneGraphNode() : SceneGraphNode(false) {}

    bool
    is_leaf() const
    {
        return m_info.index() == 0;
    }

    void
    traverse(const std::function<void(const SceneGraphLeaf &)> & func) const
    {
        if (is_leaf())
        {
            const SceneGraphLeaf & leaf_info = std::get<0>(m_info);
            func(leaf_info);
            return;
        }
        else
        {
            const std::vector<SceneGraphNode> & childs = std::get<1>(m_info);
            for (size_t i = 0; i < childs.size(); i++)
            {
                childs[i].traverse(func);
            }
            return;
        }
    }

    void
    traverse(const std::function<void(SceneGraphLeaf &)> & func)
    {
        if (is_leaf())
        {
            SceneGraphLeaf & leaf_info = std::get<0>(m_info);
            func(leaf_info);
            return;
        }
        else
        {
            std::vector<SceneGraphNode> & childs = std::get<1>(m_info);
            for (size_t i = 0; i < childs.size(); i++)
            {
                childs[i].traverse(func);
            }
            return;
        }
    }

    void
    update_transform(const float4x4 & current_transform = glm::identity<float4x4>())
    {
        const float4x4 transform = m_transform * current_transform;
        if (is_leaf())
        {
            SceneGraphLeaf & leaf_info  = std::get<0>(m_info);
            leaf_info.m_total_transform = transform;
            return;
        }
        else
        {
            std::vector<SceneGraphNode> & childs = std::get<1>(m_info);
            for (size_t i = 0; i < childs.size(); i++)
            {
                childs[i].update_transform(transform);
            }
            return;
        }
    }

    size_t
    get_num_leaves(const std::function<bool(const SceneGraphLeaf &)> & func) const
    {
        if (is_leaf())
        {
            const SceneGraphLeaf & leaf_info = std::get<0>(m_info);
            if (func(leaf_info))
            {
                return 1;
            }
            return 0;
        }
        else
        {
            const std::vector<SceneGraphNode> & childs = std::get<1>(m_info);
            size_t                              sum    = 0;
            for (size_t i = 0; i < childs.size(); i++)
            {
                sum += childs[i].get_num_leaves(func);
            }
            return sum;
        }
    }
};
