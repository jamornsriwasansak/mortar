#pragma once

#include "common/logger.h"
#include "graphicsapi/enums.h"
#include "graphicsapi/shadersrc.h"

#include <d3dcompiler.h>
#include <wrl/client.h>

#include <map>

struct HlslD3dCompiler
{
    template <typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    static inline std::map<ShaderStageEnum, LPCSTR> TargetProfile{
        { ShaderStageEnum::Vertex, "vs_5_0" },
        { ShaderStageEnum::Fragment, "ps_5_0" }
    };

    HlslD3dCompiler() {}

    ComPtr<ID3DBlob>
    compile_as_d3dblob(const ShaderSrc & shader_src, const std::vector<std::string> & defines = {})
    {
        HRESULT hr;

        ComPtr<ID3DBlob> shader_byte_code;
        ComPtr<ID3DBlob> error_buffer;

        // populate shader macros from define
        std::vector<D3D_SHADER_MACRO> shader_macros;
        shader_macros.resize(defines.size() + 1);
        for (size_t i_define = 0; i_define < defines.size(); i_define++)
        {
            shader_macros[i_define].Name       = defines[i_define].c_str();
            shader_macros[i_define].Definition = "";
        }
        shader_macros.back().Name       = NULL;
        shader_macros.back().Definition = NULL;

        // create include handler
        ID3DInclude * includer = nullptr;
        if (shader_src.m_file_path != "")
        {
            includer = D3D_COMPILE_STANDARD_FILE_INCLUDE;
        }

        // compile
        hr = D3DCompile(shader_src.m_source.c_str(),
                        shader_src.m_source.size(),
                        shader_src.m_name.c_str(),
                        shader_macros.data(),
                        includer,
                        shader_src.m_entry.data(),
                        TargetProfile[shader_src.m_shader_stage],
                        0,
                        0,
                        shader_byte_code.GetAddressOf(),
                        error_buffer.GetAddressOf());

        if (FAILED(hr) || shader_byte_code == nullptr)
        {
            Logger::Error<true>(__FUNCTION__ " compilation failed with errors \n",
                                (const char *)error_buffer->GetBufferPointer());
        }

        return shader_byte_code;
    }
};