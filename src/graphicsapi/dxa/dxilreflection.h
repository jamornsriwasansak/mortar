#pragma once

#include "common/logger.h"

#include <wrl/client.h>
// dxcapi must be included after wrl/client
#include "../inc/d3d12shader.h"
#include "../inc/dxcapi.h"

#include <DirectXMath.h>
#include <directx/d3d12.h>
#include <directx/d3dx12.h>
#include <dxgi1_6.h>

#include "../commontypes/gpshadersrc.h"

#include <map>

struct DxilReflection
{
    template <typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    struct DescriptorInfo
    {
        size_t m_root_signature_index = 0;
        size_t m_num_bindings         = 0;
    };

    struct ReflectionResult
    {
        using Space     = size_t;
        using BindPoint = size_t;
        std::list<ComPtr<ID3D12LibraryReflection>> m_library_reflections;
        std::list<ComPtr<ID3D12ShaderReflection>>  m_shader_reflections;
        std::vector<D3D12_INPUT_ELEMENT_DESC>      m_vertex_input_element_descs;
        std::list<D3D12_DESCRIPTOR_RANGE>          m_descriptor_ranges;
        std::vector<D3D12_ROOT_PARAMETER>          m_root_parameters;
        std::map<std::tuple<D3D_SHADER_INPUT_TYPE, Space, BindPoint>, DescriptorInfo> m_space_bindings;
    };

    ComPtr<ID3D12ShaderReflection>
    reflect_dxil(IDxcBlob * blob, std::optional<ComPtr<IDxcBlob>> root_signature_blob = std::nullopt) const
    {
        HRESULT                        hr;
        ComPtr<ID3D12ShaderReflection> result;

        // create container reflection
        ComPtr<IDxcContainerReflection> container_reflection;
        hr = DxcCreateInstance(CLSID_DxcContainerReflection, IID_PPV_ARGS(&container_reflection));
        if (FAILED(hr))
        {
            Logger::Critical<true>(__FUNCTION__ " failed to create container for dxil reflection");
        }

        hr = container_reflection->Load(blob);
        if (FAILED(hr))
        {
            Logger::Error<true>(__FUNCTION__ " failed to load blob");
        }

        UINT32 shader_idx;
        hr = container_reflection->FindFirstPartKind(DXC_PART_DXIL, &shader_idx);
        if (FAILED(hr))
        {
            Logger::Error<true>(__FUNCTION__ " failed to find Dxil part in the blob");
        }

        hr = container_reflection->GetPartReflection(shader_idx, IID_PPV_ARGS(&result));
        if (FAILED(hr))
        {
            Logger::Error<true>(__FUNCTION__ " failed to get Dxil part in the blob");
        }

        if (root_signature_blob.has_value())
        {
            hr = container_reflection->GetPartContent(shader_idx, &(root_signature_blob.value()));
            if (FAILED(hr))
            {
                Logger::Error<true>(__FUNCTION__ " failed to fetch root signature blob");
            }
        }

        return result;
    }

    ComPtr<ID3D12LibraryReflection>
    reflect_dxil_lib(IDxcBlob * blob, std::optional<ComPtr<IDxcBlob>> root_signature_blob = std::nullopt) const
    {
        HRESULT                         hr;
        ComPtr<ID3D12LibraryReflection> result;

        // create container reflection
        ComPtr<IDxcContainerReflection> container_reflection;
        hr = DxcCreateInstance(CLSID_DxcContainerReflection, IID_PPV_ARGS(&container_reflection));
        if (FAILED(hr))
        {
            Logger::Critical<true>(__FUNCTION__ " failed to create container for dxil reflection");
        }

        hr = container_reflection->Load(blob);
        if (FAILED(hr))
        {
            Logger::Error<true>(__FUNCTION__ " failed to load blob");
        }

        UINT32 shader_idx;
        hr = container_reflection->FindFirstPartKind(DXC_PART_DXIL, &shader_idx);
        if (FAILED(hr))
        {
            Logger::Error<true>(__FUNCTION__ " failed to find Dxil part in the blob");
        }

        hr = container_reflection->GetPartReflection(shader_idx, IID_PPV_ARGS(&result));
        if (FAILED(hr))
        {
            Logger::Error<true>(__FUNCTION__ " failed to get Dxil part in the blob");
        }

        if (root_signature_blob.has_value())
        {
            hr = container_reflection->GetPartContent(shader_idx, &(root_signature_blob.value()));
            if (FAILED(hr))
            {
                Logger::Error<true>(__FUNCTION__ " failed to fetch root signature blob");
            }
        }

        return result;
    }

    ReflectionResult
    reflect(const std::span<std::pair<ComPtr<IDxcBlob>, DXA_NAME::ShaderStageEnum>> & blobs,
            const std::vector<DXA_NAME::ShaderSrc> & shader_srcs) const
    {
        using Space     = size_t;
        using BindPoint = size_t;
        struct Binding
        {
            size_t               m_bind_count     = 0;
            D3D12_ROOT_PARAMETER m_root_parameter = {};
        };
        std::map<std::tuple<D3D_SHADER_INPUT_TYPE, Space, BindPoint>, Binding> bindings;

        ReflectionResult result;

        for (size_t i_blob = 0; i_blob < blobs.size(); i_blob++)
        {
            IDxcBlob *                blob  = blobs[i_blob].first.Get();
            DXA_NAME::ShaderStageEnum stage = blobs[i_blob].second;

            D3D12_LIBRARY_DESC         library_desc        = {};
            ID3D12LibraryReflection *  library_reflection  = nullptr;
            D3D12_FUNCTION_DESC        func_desc           = {};
            ID3D12FunctionReflection * function_reflection = nullptr;
            D3D12_SHADER_DESC          shader_desc         = {};
            ID3D12ShaderReflection *   shader_reflection   = nullptr;
            UINT                       num_bound_resources = 0;
            const bool                 is_library = stage == DXA_NAME::ShaderStageEnum::AnyHit ||
                                    stage == DXA_NAME::ShaderStageEnum::ClosestHit ||
                                    stage == DXA_NAME::ShaderStageEnum::Miss ||
                                    stage == DXA_NAME::ShaderStageEnum::RayGen ||
                                    stage == DXA_NAME::ShaderStageEnum::Intersection ||
                                    stage == DXA_NAME::ShaderStageEnum::Compute;

            // get reflection info and description
            if (is_library)
            {
                // get library desc
                result.m_library_reflections.push_back(reflect_dxil_lib(blob));
                library_reflection = result.m_library_reflections.back().Get();
                DXCK(library_reflection->GetDesc(&library_desc));

                for (UINT i_entry = 0; i_entry < library_desc.FunctionCount; i_entry++)
                {
                    // get func desc
                    function_reflection = library_reflection->GetFunctionByIndex(i_entry);
                    DXCK(function_reflection->GetDesc(&func_desc));
                    num_bound_resources = func_desc.BoundResources;

                    std::string func_name(func_desc.Name);
                    func_name = func_name.substr(0, func_name.find("@"));

                    if (func_name == "\x1?" + shader_srcs[i_blob].m_entry)
                    {
                        break;
                    }

                    if (i_entry + 1 == library_desc.FunctionCount)
                    {
                        Logger::Error<true>(__FUNCTION__,
                                            " cannot not find entry ",
                                            shader_srcs[i_blob].m_entry,
                                            " in library created from ",
                                            shader_srcs[i_blob].m_file_path);
                    }
                }
            }
            else
            {
                // get shader desc
                result.m_shader_reflections.push_back(reflect_dxil(blob));
                shader_reflection = result.m_shader_reflections.back().Get();
                shader_reflection->GetDesc(&shader_desc);
                num_bound_resources = shader_desc.BoundResources;
            }

            // if vertex shader, also create input element descriptions
            if (stage == DXA_NAME::ShaderStageEnum::Vertex)
            {
                result.m_vertex_input_element_descs.resize(shader_desc.InputParameters);
                for (size_t i = 0; i < shader_desc.InputParameters; i++)
                {
                    D3D12_SIGNATURE_PARAMETER_DESC param;
                    DXCK(shader_reflection->GetInputParameterDesc(static_cast<UINT>(i), &param));
                    result.m_vertex_input_element_descs[i] = get_suitable_input_element_desc(param);
                }
            }

            // iterate bound resources
            for (size_t i_binding = 0; i_binding < num_bound_resources; i_binding++)
            {
                // get binding desc
                D3D12_SHADER_INPUT_BIND_DESC binding_desc;
                if (is_library)
                {
                    DXCK(function_reflection->GetResourceBindingDesc(static_cast<UINT>(i_binding), &binding_desc));
                }
                else
                {
                    DXCK(shader_reflection->GetResourceBindingDesc(static_cast<UINT>(i_binding), &binding_desc));
                }

                // tuple
                std::tuple<D3D_SHADER_INPUT_TYPE, int, int> index = { binding_desc.Type,
                                                                      binding_desc.Space,
                                                                      binding_desc.BindPoint };

                // reflection for root signature
                const auto & iter = bindings.find(index);
                if (iter == bindings.end())
                {
                    // assign binding desc
                    bindings[index].m_bind_count = static_cast<size_t>(binding_desc.BindCount);
                    bindings[index].m_root_parameter =
                        get_suitable_root_parameter(&result.m_descriptor_ranges, binding_desc, stage);
                }
                else
                {
                    // modify existing root parameter
                    bindings[index].m_root_parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
                }
            }
        }

        size_t i_root_param = 0;
        for (const auto & binding : bindings)
        {
            result.m_root_parameters.push_back(binding.second.m_root_parameter);

            // establish descriptor info
            DescriptorInfo info                    = {};
            info.m_num_bindings                    = binding.second.m_bind_count;
            info.m_root_signature_index            = i_root_param++;
            result.m_space_bindings[binding.first] = info;
        }

        return result;
    }

private:
    D3D12_SHADER_VISIBILITY
    get_shader_visibility(const DXA_NAME::ShaderStageEnum shader_stage) const
    {
        switch (shader_stage)
        {
        case DXA_NAME::ShaderStageEnum::Vertex:
            return D3D12_SHADER_VISIBILITY_VERTEX;
        case DXA_NAME::ShaderStageEnum::Fragment:
            return D3D12_SHADER_VISIBILITY_PIXEL;
        case DXA_NAME::ShaderStageEnum::RayGen:
        case DXA_NAME::ShaderStageEnum::ClosestHit:
        case DXA_NAME::ShaderStageEnum::AnyHit:
        case DXA_NAME::ShaderStageEnum::Miss:
        case DXA_NAME::ShaderStageEnum::Intersection:
            return D3D12_SHADER_VISIBILITY_ALL;
        default:
            Logger::Critical<true>(__FUNCTION__,
                                   " unknown shader stage ",
                                   std::to_string(static_cast<int>(shader_stage)));
            return D3D12_SHADER_VISIBILITY_ALL;
        }
    }

    D3D12_ROOT_PARAMETER
    get_suitable_root_parameter(std::list<D3D12_DESCRIPTOR_RANGE> *  desc_ranges,
                                const D3D12_SHADER_INPUT_BIND_DESC & binding_desc,
                                const DXA_NAME::ShaderStageEnum      shader_stage) const
    {
        // add root parameter
        CD3DX12_ROOT_PARAMETER root_parameter = {};
        switch (binding_desc.Type)
        {
        case D3D_SIT_CBUFFER:
        {
            if (binding_desc.BindCount == 1)
            {
                root_parameter.InitAsConstantBufferView(binding_desc.BindPoint,
                                                        binding_desc.Space,
                                                        get_shader_visibility(shader_stage));
                break;
            }
            else
            {
                CD3DX12_DESCRIPTOR_RANGE descriptor_range;
                descriptor_range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
                                      binding_desc.BindCount,
                                      binding_desc.BindPoint,
                                      binding_desc.Space);
                desc_ranges->push_back(descriptor_range);
                root_parameter.InitAsDescriptorTable(1, &desc_ranges->back(), get_shader_visibility(shader_stage));
                break;
            }
        }
        case D3D_SIT_TEXTURE:
        {
            /*if (binding_desc.BindCount == 1)
            {
                root_parameter.InitAsShaderResourceView(binding_desc.BindPoint,
                                                        binding_desc.Space,
                                                        get_shader_visibility(stage));
                break;
            }
            else*/
            {
                CD3DX12_DESCRIPTOR_RANGE descriptor_range;
                descriptor_range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
                                      binding_desc.BindCount,
                                      binding_desc.BindPoint,
                                      binding_desc.Space);
                desc_ranges->push_back(descriptor_range);
                root_parameter.InitAsDescriptorTable(1, &desc_ranges->back(), get_shader_visibility(shader_stage));
                break;
            }
        }
        case D3D_SIT_SAMPLER:
        {
            CD3DX12_DESCRIPTOR_RANGE descriptor_range;
            descriptor_range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,
                                  binding_desc.BindCount,
                                  binding_desc.BindPoint,
                                  binding_desc.Space);
            desc_ranges->push_back(descriptor_range);
            root_parameter.InitAsDescriptorTable(1, &desc_ranges->back(), get_shader_visibility(shader_stage));
            break;
        }
        case D3D_SIT_UAV_RWTYPED:
        {
            /*
            root_parameter.InitAsUnorderedAccessView(binding_desc.BindPoint,
                                                     binding_desc.Space,
                                                     get_shader_visibility(shader_stage));*/
            CD3DX12_DESCRIPTOR_RANGE descriptor_range;
            descriptor_range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
                                  binding_desc.BindCount,
                                  binding_desc.BindPoint,
                                  binding_desc.Space);
            desc_ranges->push_back(descriptor_range);
            root_parameter.InitAsDescriptorTable(1, &desc_ranges->back(), get_shader_visibility(shader_stage));
            break;
        }
        case D3D_SIT_UAV_RWSTRUCTURED:
        {
            /*
            root_parameter.InitAsUnorderedAccessView(binding_desc.BindPoint,
                                                     binding_desc.Space,
                                                     get_shader_visibility(shader_stage));*/
            CD3DX12_DESCRIPTOR_RANGE descriptor_range;
            descriptor_range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
                                  binding_desc.BindCount,
                                  binding_desc.BindPoint,
                                  binding_desc.Space);
            desc_ranges->push_back(descriptor_range);
            root_parameter.InitAsDescriptorTable(1, &desc_ranges->back(), get_shader_visibility(shader_stage));
            break;
        }
        case D3D_SIT_RTACCELERATIONSTRUCTURE:
        {
            CD3DX12_DESCRIPTOR_RANGE descriptor_range;
            descriptor_range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
                                  binding_desc.BindCount,
                                  binding_desc.BindPoint,
                                  binding_desc.Space);
            desc_ranges->push_back(descriptor_range);
            root_parameter.InitAsDescriptorTable(1, &desc_ranges->back(), get_shader_visibility(shader_stage));
            /*
            root_parameter.InitAsShaderResourceView(binding_desc.BindPoint,
                                                    binding_desc.Space,
                                                    get_shader_visibility(shader_stage));
                                                    */
            break;
        }
        case D3D_SIT_BYTEADDRESS:
        {
            CD3DX12_DESCRIPTOR_RANGE descriptor_range;
            descriptor_range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
                                  binding_desc.BindCount,
                                  binding_desc.BindPoint,
                                  binding_desc.Space);
            desc_ranges->push_back(descriptor_range);
            root_parameter.InitAsDescriptorTable(1, &desc_ranges->back(), get_shader_visibility(shader_stage));
            /*
            root_parameter.InitAsShaderResourceView(binding_desc.BindPoint,
                                                    binding_desc.Space,
                                                    get_shader_visibility(shader_stage));
                                                    */
            break;
        }
        case D3D_SIT_STRUCTURED:
        {
            CD3DX12_DESCRIPTOR_RANGE descriptor_range;
            descriptor_range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
                                  binding_desc.BindCount,
                                  binding_desc.BindPoint,
                                  binding_desc.Space);
            desc_ranges->push_back(descriptor_range);
            root_parameter.InitAsDescriptorTable(1, &desc_ranges->back(), get_shader_visibility(shader_stage));
            break;
        }
        default:
        {
            Logger::Critical<true>(__FUNCTION__, " found unsupported type");
            break;
        }
        }

        return root_parameter;
    }

    D3D12_INPUT_ELEMENT_DESC
    get_suitable_input_element_desc(const D3D12_SIGNATURE_PARAMETER_DESC & param) const
    {
        D3D12_INPUT_ELEMENT_DESC input_element_desc = {};
        {
            input_element_desc.SemanticName         = param.SemanticName;
            input_element_desc.SemanticIndex        = param.SemanticIndex;
            input_element_desc.InputSlot            = 0;
            input_element_desc.AlignedByteOffset    = D3D12_APPEND_ALIGNED_ELEMENT;
            input_element_desc.InputSlotClass       = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
            input_element_desc.InstanceDataStepRate = 0;
        }

        size_t num_comps = 0;
        if (param.Mask >= 15)
        {
            num_comps = 4;
        }
        else if (param.Mask >= 7)
        {
            num_comps = 3;
        }
        else if (param.Mask >= 3)
        {
            num_comps = 2;
        }
        else if (param.Mask >= 1)
        {
            num_comps = 1;
        }

        if (param.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32)
        {
            switch (num_comps)
            {
            case 1:
                input_element_desc.Format = DXGI_FORMAT_R32_FLOAT;
                break;
            case 2:
                input_element_desc.Format = DXGI_FORMAT_R32G32_FLOAT;
                break;
            case 3:
                input_element_desc.Format = DXGI_FORMAT_R32G32B32_FLOAT;
                break;
            case 4:
                input_element_desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
                break;
            default:
                Logger::Critical<true>(__FUNCTION__, " unsupported type");
                break;
            }
        }
        else if (param.ComponentType == D3D_REGISTER_COMPONENT_UINT32)
        {
            switch (num_comps)
            {
            case 1:
                input_element_desc.Format = DXGI_FORMAT_R32_UINT;
                break;
            case 2:
                input_element_desc.Format = DXGI_FORMAT_R32G32_UINT;
                break;
            case 3:
                input_element_desc.Format = DXGI_FORMAT_R32G32B32_UINT;
                break;
            case 4:
                input_element_desc.Format = DXGI_FORMAT_R32G32B32A32_UINT;
                break;
            default:
                Logger::Critical<true>(__FUNCTION__, " unsupported type");
                break;
            }
        }
        else
        {
            Logger::Critical<true>(__FUNCTION__, " unsupported type");
        }

        return input_element_desc;
    }
};