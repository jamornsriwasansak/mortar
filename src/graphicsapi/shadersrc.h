#pragma once

#include "common/file.h"
#include "graphicsapi/enums.h"

#include <filesystem>
#include <string>
#include <vector>

template <typename ShaderStageEnum>
struct TShaderSrc
{
    std::filesystem::path    m_file_path = "";
    std::string              m_source    = "";
    std::string              m_entry     = "";
    std::vector<std::string> m_defines;
    ShaderStageEnum          m_shader_stage;

    TShaderSrc() {}

    TShaderSrc(const ShaderStageEnum         shader_stage,
               const std::filesystem::path & path   = "",
               const std::string &           source = "")
    : m_shader_stage(shader_stage), m_file_path(path), m_source(source)
    {
    }

    TShaderSrc &
    set_file_path(const std::filesystem::path & path)
    {
        m_file_path = path;
        return *this;
    }

    TShaderSrc &
    set_raw_source(const std::string & source)
    {
        m_source = source;
        return *this;
    }

    TShaderSrc &
    set_entry(const std::string & entry)
    {
        m_entry = entry;
        return *this;
    }

    std::string
    source() const
    {
        if (m_file_path != "")
        {
            return File::LoadFile(m_file_path.c_str());
        }
        else
        {
            return m_source;
        }
    }
};

/*
template <typename T>
struct TGraphicsPipelineShaderSrc
{
    std::vector<TShaderSrc<T>> m_shader_srcs;

    TGraphicsPipelineTShaderSrc() {}

    TShaderSrc<T> &
    get(const Dxa::ShaderStageEnum shader_stage)
    {
        for (auto & shader_src : m_shader_srcs)
        {
            if (shader_src.m_shader_stage == shader_stage)
            {
                return shader_src;
            }
        }
        m_shader_srcs.push_back(TShaderSrc<T>(shader_stage));
        return m_shader_srcs.back();
    }

    TShaderSrc<T> &
    vs()
    {
        return get(T::Vertex);
    }

    TShaderSrc<T> &
    fs()
    {
        return get(T::Fragment);
    }
};
*/
