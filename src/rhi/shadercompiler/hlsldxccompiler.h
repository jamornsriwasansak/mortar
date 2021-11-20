#pragma once

#include "rhi/commontypes/rhienums.h"
#include "rhi/commontypes/rhishadersrc.h"
//
#include <wrl/client.h>
// dxcapi must be included after wrl/client
#include "../inc/dxcapi.h"
//
#include "core/logger.h"
#include "core/vmath.h"
//
#include <filesystem>
#include <map>
#include <string>

struct HlslDxcCompiler
{
    static constexpr uint32_t BShift = 0;
    static constexpr uint32_t UShift = 10;
    static constexpr uint32_t TShift = 20;
    static constexpr uint32_t SShift = 30;

    template <typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    ComPtr<IDxcUtils>    m_utils;
    ComPtr<IDxcCompiler> m_compiler;

    HlslDxcCompiler()
    {
        HRESULT hr;

        // create dxc utils
        hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(m_utils.GetAddressOf()));
        if (FAILED(hr))
        {
            Logger::Error<true>(__FUNCTION__ " failed to create DxcUtils");
        }

        // create compiler
        hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(m_compiler.GetAddressOf()));
        if (FAILED(hr))
        {
            Logger::Error<true>(__FUNCTION__ " failed to create DxcCompiler");
        }
    }

    template <typename ShaderStageEnum>
    static const wchar_t *
    get_target_profile(const ShaderStageEnum shader_stage)
    {
        switch (shader_stage)
        {
        case ShaderStageEnum::Vertex:
            return L"vs_6_2";
        case ShaderStageEnum::Fragment:
            return L"ps_6_2";
        default:
            return L"lib_6_5";
        }
    }

    template <bool AsSpirV>
    static std::vector<LPCWSTR> *
    get_compiling_argument()
    {
        static std::vector<LPCWSTR> arguments = [&]() {
            std::vector<LPCWSTR> result;
            result.push_back(DXC_ARG_DEBUG);
            result.push_back(DXC_ARG_ALL_RESOURCES_BOUND);
            result.push_back(DXC_ARG_OPTIMIZATION_LEVEL3);
            result.push_back(DXC_ARG_WARNINGS_ARE_ERRORS);
            result.push_back(L"-enable-16bit-types");
            // result.push_back(L"-enable-templates");
            result.push_back(L"-HV 2018");
            result.push_back(L"-T *s_6_2");

            // compile as spirv if needed
            if constexpr (AsSpirV)
            {
                static const std::wstring bshift = std::to_wstring(BShift);
                static const std::wstring ushift = std::to_wstring(UShift);
                static const std::wstring tshift = std::to_wstring(TShift);
                static const std::wstring sshift = std::to_wstring(SShift);

                result.push_back(L"-fvk-auto-shift-bindings");
                result.push_back(L"-fspv-reflect");
                result.push_back(L"-spirv");
                result.push_back(L"-fspv-target-env=vulkan1.2");

                // force storage buffer and constant buffer to not use std140 and std430
                // and simply use dx-like (VK_EXT_scalar_block_layout) standard instead
                // this renders padding unnecessary.
                // note: this requires vk::Device::setScalarBlockLayout(VK_TRUE)
                result.push_back(L"-fvk-use-scalar-layout");

                // constant buffer - 0
                result.push_back(L"-fvk-b-shift");
                result.push_back(bshift.c_str());
                result.push_back(L"all");

                // sampler - 10
                result.push_back(L"-fvk-u-shift");
                result.push_back(ushift.c_str());
                result.push_back(L"all");

                // srv - 20
                result.push_back(L"-fvk-t-shift");
                result.push_back(tshift.c_str());
                result.push_back(L"all");

                // sampler - 30
                result.push_back(L"-fvk-s-shift");
                result.push_back(sshift.c_str());
                result.push_back(L"all");

                // hlsl in general
                result.push_back(L"-fspv-extension=SPV_GOOGLE_hlsl_functionality1");
                result.push_back(L"-fspv-extension=SPV_GOOGLE_user_type");
                result.push_back(L"-fspv-extension=KHR");
            }
            return result;
        }();

        return &arguments;
    }

    template <typename ShaderStageEnum>
    ComPtr<IDxcBlob>
    dxc_compile(const std::string &              shader_name,
                const std::string &              shader_string,
                const std::string &              shader_access_point,
                const ShaderStageEnum            shader_stage,
                const std::filesystem::path &    path,
                const std::vector<std::string> & defines,
                const bool                       as_spirv) const
    {
        HRESULT hr;

        // name of dxc must be path so includer knows relative path
        std::wstring wshader_name(shader_name.begin(), shader_name.end());
        std::wstring wshader_path = path.wstring();

        // create blob from shader string
        ComPtr<IDxcBlobEncoding> source_blob;
        hr = m_utils->CreateBlobFromPinned(shader_string.data(),
                                           static_cast<UINT32>(shader_string.size()),
                                           DXC_CP_UTF8,
                                           source_blob.GetAddressOf());
        if (FAILED(hr))
        {
            Logger::Error<true>(
                __FUNCTION__ " failed to create shader string (UTF8) blob caused by " + path.string());
        }

        // create include handler
        ComPtr<IDxcIncludeHandler> includer;
        hr = m_utils->CreateDefaultIncludeHandler(includer.GetAddressOf());
        if (FAILED(hr))
        {
            Logger::Error<true>(
                __FUNCTION__ " failed to create default includer header caused by : " + path.string());
        }

        // convert string of defines into string of wide strings
        std::vector<std::wstring> wdefines;
        wdefines.reserve(defines.size());
        for (size_t i = 0; i < defines.size(); i++)
        {
            wdefines.emplace_back(defines[i].begin(), defines[i].end());
        }

        // plug wide strings into dxc
        std::vector<DxcDefine> dxc_defines;
        dxc_defines.reserve(wdefines.size() + 2);
        for (size_t i = 0; i < wdefines.size(); i++)
        {
            DxcDefine dxcdefine;
            dxcdefine.Name  = wdefines[i].c_str();
            dxcdefine.Value = nullptr;
            dxc_defines.push_back(dxcdefine);
        }

        // default dxc defines
        DxcDefine dxc_preprocessor;
        dxc_preprocessor.Name  = L"__dxc";
        dxc_preprocessor.Value = nullptr;
        dxc_defines.push_back(dxc_preprocessor);

        DxcDefine hlsl_preprocessor;
        hlsl_preprocessor.Name  = L"__hlsl";
        hlsl_preprocessor.Value = nullptr;
        dxc_defines.push_back(hlsl_preprocessor);

        // populate arguments (dxc command-line arguments)
        std::vector<LPCWSTR> * arguments;
        if (as_spirv)
        {
            arguments = get_compiling_argument<true>();
        }
        else
        {
            arguments = get_compiling_argument<false>();
        }


        std::wstring wshader_access_point(shader_access_point.begin(), shader_access_point.end());

        // dxc preprocess
        ComPtr<IDxcBlob> preprocessed_source_blob;
        {
            ComPtr<IDxcOperationResult> preprocess_result;
            hr = m_compiler->Preprocess(source_blob.Get(),
                                        wshader_path.c_str(),
                                        arguments->data(),
                                        static_cast<UINT32>(arguments->size()),
                                        dxc_defines.data(),
                                        static_cast<UINT32>(dxc_defines.size()),
                                        includer.Get(),
                                        &preprocess_result);

            // check if succeed
            if (SUCCEEDED(hr)) preprocess_result->GetStatus(&hr);

            // handle compilation fail
            if (FAILED(hr) || preprocess_result == nullptr)
            {
                if (preprocess_result)
                {
                    ComPtr<IDxcBlobEncoding> error_blob;
                    HRESULT                  hr2 = preprocess_result->GetErrorBuffer(&error_blob);
                    if (SUCCEEDED(hr2) && error_blob)
                    {
                        Logger::Error<true>(__FUNCTION__,
                                            " compilation failed with errors caused by",
                                            path.string(),
                                            "\n",
                                            (const char *)error_blob->GetBufferPointer());
                    }
                    else
                    {
                        Logger::Error<true>(
                            __FUNCTION__ " preprocessing error creation fail 0 caused by",
                            path.string());
                    }
                }
                else
                {
                    Logger::Error<true>(
                        __FUNCTION__ " preprocessing error creation fail 1 caused by",
                        path.string());
                }
            }
            hr = preprocess_result->GetResult(&preprocessed_source_blob);
            if (FAILED(hr))
            {
                Logger::Error<true>(
                    __FUNCTION__ " failed to create preprocessed code blob caused by",
                    path.string());
            }
        }

        // dxc compile
        ComPtr<IDxcBlob> code_result;
        {
            ComPtr<IDxcOperationResult> compilation_result;

            hr = m_compiler->Compile(preprocessed_source_blob.Get(),
                                     wshader_name.c_str(),
                                     wshader_access_point.c_str(),
                                     get_target_profile(shader_stage),
                                     arguments->data(),
                                     static_cast<UINT32>(arguments->size()),
                                     dxc_defines.data(),
                                     static_cast<UINT32>(dxc_defines.size()),
                                     includer.Get(),
                                     &compilation_result);

            // check if succeed
            if (SUCCEEDED(hr)) compilation_result->GetStatus(&hr);

            // handle compilation fail
            if (FAILED(hr) || compilation_result == nullptr)
            {
                if (compilation_result)
                {
                    ComPtr<IDxcBlobEncoding> error_blob;
                    HRESULT                  hr2 = compilation_result->GetErrorBuffer(&error_blob);
                    if (SUCCEEDED(hr2) && error_blob)
                    {
                        Logger::Error<true>(__FUNCTION__,
                                            " compilation failed with errors caused by",
                                            path.string(),
                                            "\n",
                                            (const char *)error_blob->GetBufferPointer());
                    }
                    else
                    {
                        Logger::Error<true>(
                            __FUNCTION__ " compilation error creation fail 0 caused by",
                            path.string());
                    }
                }
                else
                {
                    Logger::Error<true>(__FUNCTION__ " compilation error creation fail 1 caused by",
                                        path.string());
                }
            }

            hr = compilation_result->GetResult(&code_result);
            if (FAILED(hr))
            {
                Logger::Error<true>(__FUNCTION__ " failed to create code result blob caused by", path.string());
            }
        }

        return code_result;
    }

    template <typename ShaderStageEnum>
    ComPtr<IDxcBlob>
    compile_as_dxil(const Rhi::TShaderSrc<ShaderStageEnum> & shader_src,
                    const std::vector<std::string> &         defines = {}) const
    {
        Logger::Info(__FUNCTION__ " compiling dxil from path : " + shader_src.m_file_path.string());
        return dxc_compile(shader_src.m_file_path.string(),
                           shader_src.source(),
                           shader_src.m_entry,
                           shader_src.m_shader_stage,
                           shader_src.m_file_path,
                           defines,
                           false);
    }

    template <typename ShaderStageEnum>
    std::vector<uint32_t>
    compile_as_spirv(const Rhi::TShaderSrc<ShaderStageEnum> & shader_src,
                     const std::vector<std::string> &         defines = {}) const
    {
        Logger::Info(__FUNCTION__ " compiling spirv from path : " + shader_src.m_file_path.string());

        ComPtr<IDxcBlob> blob = dxc_compile(shader_src.m_file_path.string(),
                                            shader_src.source(),
                                            shader_src.m_entry,
                                            shader_src.m_shader_stage,
                                            shader_src.m_file_path,
                                            defines,
                                            true);
        assert(blob->GetBufferSize() % 4 == 0);
        std::vector<uint32_t> code(div_ceil(blob->GetBufferSize(), 4), 0);
        std::memcpy(code.data(), blob->GetBufferPointer(), blob->GetBufferSize());
        return code;
    }
};
