#pragma once

#include "rhi/commontypes/rhienums.h"
#include "rhi/commontypes/rhishadersrc.h"
//
#include <wrl/client.h>
// dxcapi must be included after wrl/client
#include "../inc/dxcapi.h"
//

#include "rhi/shadercompiler/hlsldxccompiler.h"

struct SimpleDxcBlob : public IDxcBlob
{
    std::vector<std::byte> m_data;

    SimpleDxcBlob() {}

    SimpleDxcBlob(IDxcBlob * dxc_blob)
    {
        m_data.resize(dxc_blob->GetBufferSize());
        std::memcpy(m_data.data(), dxc_blob->GetBufferPointer(), dxc_blob->GetBufferSize());
    }

    LPVOID STDMETHODCALLTYPE
    GetBufferPointer() override
    {
        return m_data.data();
    }

    SIZE_T STDMETHODCALLTYPE
    GetBufferSize() override
    {
        return m_data.size();
    }
};

// Shader Manager
// cache all shaders and
struct ShaderManager
{
    template <typename T>
    using ComPtr    = Microsoft::WRL::ComPtr<T>;
    using ShaderSrc = Rhi::TShaderSrc<Rhi::ShaderStageEnum>;

    HlslDxcCompiler m_hlsl_compiler;

    struct ShaderCacheFileHeader
    {
        uint32_t m_version;
        uint32_t m_compiled_shader_size;
        uint64_t m_src_last_modified_time;
    };

    struct ShaderCacheFileBody
    {
        std::vector<std::byte> m_data;
    };

    struct ShaderCacheFile
    {
        ShaderCacheFileHeader m_header;
        ShaderCacheFileBody   m_body;
    };

    std::filesystem::path   m_cache_folder;

    ShaderManager() {}

    ShaderManager(const std::filesystem::path & cache_folder) : m_cache_folder(cache_folder) {}

    // ShaderBlob
    std::vector<std::byte>
    get_cached_shader(const ShaderSrc & shader_src) const
    {
        // no filepath, compile directly from shader src
        if (shader_src.m_file_path.empty())
        {
            ComPtr<IDxcBlob> dxc_blob = m_hlsl_compiler.compile_as_spirv(shader_src);
            return to_byte_vector(*dxc_blob.Get());
        }

        // check source write time
        const uint64_t src_modified_time = static_cast<uint64_t>(
            std::filesystem::last_write_time(shader_src.m_file_path).time_since_epoch().count());

        // get shader blob
        const std::filesystem::path cached_file_path = get_shader_cache_path(shader_src);
        std::ifstream               ifs(cached_file_path, std::ios::binary);
        if (ifs.is_open())
        {
            // read header and body
            const ShaderCacheFileHeader header = read_header(ifs);
            if (header.m_src_last_modified_time == src_modified_time)
            {
                const ShaderCacheFileBody body = read_body(header, ifs);
                return body.m_data;
            }
        }

        // cached dxc blob is invalid.
        // we have to recompile and rewrite the cache
        ComPtr<IDxcBlob> dxc_blob = m_hlsl_compiler.compile_as_spirv(shader_src);

        // write header and body
        std::ofstream ofs(cached_file_path, std::ios::binary);
        write_header(ofs, src_modified_time, dxc_blob.Get());
        write_body(ofs, dxc_blob.Get());

        return to_byte_vector(*dxc_blob.Get());
    }

private:
    std::filesystem::path
    get_shader_cache_path(const ShaderSrc & shader_src) const
    {
        std::string val = shader_src.m_file_path.string();
        val += "_" + shader_src.m_entry;
        val += "_";
        for (const std::string & define : shader_src.m_defines)
        {
            val += define;
        }
        const size_t unique_id = std::hash<std::string>()(val);

        return m_cache_folder / (std::to_string(unique_id) + ".shaderbin");
    }

    std::vector<std::byte>
    to_byte_vector(IDxcBlob & blob) const
    {
        const size_t           size = static_cast<size_t>(blob.GetBufferSize());
        std::vector<std::byte> result(size);
        std::memcpy(&result[0], blob.GetBufferPointer(), size);
        return result;
    }

    ShaderCacheFileHeader
    read_header(std::ifstream & ifs) const
    {
        assert(ifs.is_open());
        ShaderCacheFileHeader result;
        ifs.read(reinterpret_cast<char *>(&result.m_version), sizeof(result.m_version))
            .read(reinterpret_cast<char *>(&result.m_src_last_modified_time), sizeof(result.m_src_last_modified_time))
            .read(reinterpret_cast<char *>(&result.m_compiled_shader_size), sizeof(result.m_compiled_shader_size));
        return result;
    }

    ShaderCacheFileBody
    read_body(const ShaderCacheFileHeader & header, std::ifstream & ifs) const
    {
        assert(ifs.is_open());
        ShaderCacheFileBody result;
        result.m_data.resize(header.m_compiled_shader_size);
        ifs.read(reinterpret_cast<char *>(result.m_data.data()), result.m_data.size());
        return result;
    }

    void
    write_header(std::ofstream & ofs, const uint64_t src_modified_time, IDxcBlob * blob) const
    {
        assert(ofs.is_open());
        ShaderCacheFileHeader header;
        header.m_version                = 0;
        header.m_compiled_shader_size   = blob->GetBufferSize();
        header.m_src_last_modified_time = src_modified_time;
        ofs.write(reinterpret_cast<char *>(&header.m_version), sizeof(header.m_version))
            .write(reinterpret_cast<char *>(&header.m_src_last_modified_time), sizeof(header.m_src_last_modified_time))
            .write(reinterpret_cast<char *>(&header.m_compiled_shader_size),
                   sizeof(header.m_compiled_shader_size));
    }

    void
    write_body(std::ofstream & ofs, IDxcBlob * blob) const
    {
        assert(ofs.is_open());
        ShaderCacheFileBody result;
        result.m_data.resize(blob->GetBufferSize());
        ofs.write(reinterpret_cast<char *>(blob->GetBufferPointer()), result.m_data.size());
    }
};