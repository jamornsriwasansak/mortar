#pragma once

#include "core/file.h"
#include "rhienums.h"

#include <filesystem>
#include <string>
#include <vector>

namespace Rhi
{
template <typename ShaderStageEnum>
struct TShaderSrc
{
    std::filesystem::path m_file_path = "";
    std::string m_entry               = "";
    std::vector<std::string> m_defines;
    ShaderStageEnum m_shader_stage;

    TShaderSrc() {}

    TShaderSrc(const ShaderStageEnum shader_stage,
               const std::filesystem::path & path = "",
               const std::string & entry          = "")
    : m_shader_stage(shader_stage), m_file_path(path), m_entry(entry)
    {
    }

    TShaderSrc &
    set_file_path(const std::filesystem::path & path)
    {
        m_file_path = path;
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
        return File::LoadFile(m_file_path.c_str());
    }
};
} // namespace Rhi