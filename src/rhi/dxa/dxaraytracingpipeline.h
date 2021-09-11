#pragma once

#include "dxacommon.h"
#include "dxilreflection.h"

#include "rhi/commontypes/rhishadersrc.h"
#include "rhi/shadercompiler/hlsldxccompiler.h"

namespace DXA_NAME
{
struct RayTracingHitGroup
{
    std::optional<size_t> m_closest_hit_id;
    std::optional<size_t> m_any_hit_id;
    std::optional<size_t> m_intersect_id;
};

struct RayTracingPipelineConfig
{
    std::vector<ShaderSrc>          m_shader_srcs;
    std::vector<RayTracingHitGroup> m_hit_groups;

    RayTracingPipelineConfig() {}

    size_t
    add_shader(const ShaderSrc & shader_src)
    {
        // push back
        m_shader_srcs.push_back(shader_src);
        return m_shader_srcs.size() - 1;
    }

    [[deprecated]] size_t
    add_hit_group(const size_t closest_hit_id,
                  const size_t any_hit_id   = std::numeric_limits<size_t>::max(),
                  const size_t intersect_id = std::numeric_limits<size_t>::max())
    {
        RayTracingHitGroup hit_group;
        hit_group.m_closest_hit_id = closest_hit_id;
        if (any_hit_id != std::numeric_limits<size_t>::max())
        {
            hit_group.m_any_hit_id = any_hit_id;
        }
        if (intersect_id != std::numeric_limits<size_t>::max())
        {
            hit_group.m_intersect_id = intersect_id;
        }
        m_hit_groups.push_back(hit_group);
        return m_hit_groups.size() - 1;
    }

    size_t
    add_hit_group(const RayTracingHitGroup & hit_group)
    {
        m_hit_groups.push_back(hit_group);
        return m_hit_groups.size() - 1;
    }
};

struct RayTracingPipeline
{
    ComPtr<ID3D12StateObject>           m_dx_rt_pso;
    ComPtr<ID3D12StateObjectProperties> m_dx_rt_pso_props;

    using RootSignatureIndex                               = size_t;
    using BindPoint                                        = size_t;
    using SpaceIndex                                       = size_t;
    ComPtr<ID3D12RootSignature> m_dx_global_root_signature = nullptr;
    std::map<std::tuple<D3D_SHADER_INPUT_TYPE, SpaceIndex, BindPoint>, DxilReflection::DescriptorInfo> m_descriptor_set_info;

    size_t                    m_raygen_record_size    = 0;
    size_t                    m_miss_record_size      = 0;
    size_t                    m_hit_group_record_size = 0;
    size_t                    m_num_raygens           = 0;
    size_t                    m_num_misses            = 0;
    size_t                    m_num_hit_groups        = 0;
    std::vector<std::wstring> m_raygen_renamed_symbols;
    std::vector<std::wstring> m_miss_renamed_symbols;
    std::vector<std::wstring> m_hit_group_renamed_symbols;

    struct ShaderEntry
    {
        ComPtr<ID3D12RootSignature> m_local_root_signature = nullptr;
        ComPtr<IDxcBlob>            m_compiled_shader_blob = nullptr;
        size_t                      m_num_root_parameters  = 0;
        std::wstring                m_entry_symbol = L""; // name of the shader entry to be exported
        std::wstring m_renamed_symbol = L""; // name after the shader entry after renamed

        using BindPoint  = size_t;
        using SpaceIndex = size_t;
        std::map<std::tuple<D3D_SHADER_INPUT_TYPE, SpaceIndex, BindPoint>, DxilReflection::DescriptorInfo> m_descriptor_set_info;
    };

    struct HitGroupRecord
    {
        std::optional<size_t> m_closest_hit_id;
        std::optional<size_t> m_any_hit_id;
        std::optional<size_t> m_intersect_id;
        std::wstring          m_symbol = L"";
    };

    RayTracingPipeline() {}

    RayTracingPipeline(const Device *                   device,
                       const RayTracingPipelineConfig & rt_lib,
                       const size_t                     attribute_size,
                       const size_t                     payload_size,
                       const size_t                     recursion_depth,
                       const std::string &              name = "")
    {
        HlslDxcCompiler hlsl_dxil_compiler;
        DxilReflection  dxil_reflector;

        std::vector<ShaderEntry> shader_entries(rt_lib.m_shader_srcs.size());
        for (size_t i = 0; i < rt_lib.m_shader_srcs.size(); i++)
        {
            // local root signature
            // this feature is disabled because it is unsupported in VK
            /*
            const auto reflection_result = m_dxil_reflector.reflect(dxc_blob, shader_src.m_shader_stage);

            // create local root signature and check for errors
            ComPtr<ID3D12RootSignature> root_signature      = nullptr;
            CD3DX12_ROOT_SIGNATURE_DESC root_signature_desc = {};
            if (shader_src.m_shader_stage == ShaderStageEnum::RayGen ||
                shader_src.m_shader_stage == ShaderStageEnum::ClosestHit ||
                shader_src.m_shader_stage == ShaderStageEnum::Miss)
            {
                root_signature_desc.Init(static_cast<UINT>(reflection_result.m_root_parameters.size()),
                                         reflection_result.m_root_parameters.data(),
                                         0,
                                         nullptr,
                                         D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE);
                ComPtr<ID3DBlob> signature = nullptr;
                ComPtr<ID3DBlob> error     = nullptr;
                HRESULT          root_description_create_result =
                    D3D12SerializeRootSignature(&root_signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
                if (error != nullptr)
                {
                    Logger::Critical<true>(__FUNCTION__ " ", static_cast<char *>(error->GetBufferPointer()));
                }
                DXCK(root_description_create_result);
                DXCK(DDev(0)->CreateRootSignature(0,
                                                  signature->GetBufferPointer(),
                                                  signature->GetBufferSize(),
                                                  IID_PPV_ARGS(&root_signature)));
            }
            */

            // entry
            const ShaderSrc & shader_src = rt_lib.m_shader_srcs[i];
            ShaderEntry &     entry      = shader_entries[i];

            const size_t      unique_id      = i;
            const std::string renamed_symbol = shader_src.m_entry + "_" + std::to_string(unique_id);

            // compile shader blob
            ComPtr<IDxcBlob> dxc_blob =
                hlsl_dxil_compiler.compile_as_dxil(rt_lib.m_shader_srcs[i], rt_lib.m_shader_srcs[i].m_defines);

            entry.m_compiled_shader_blob = dxc_blob;
            // entry.m_num_root_parameters  = root_signature_desc.NumParameters;
            // entry.m_local_root_signature = root_signature;
            entry.m_num_root_parameters  = 0;
            entry.m_local_root_signature = nullptr;
            entry.m_entry_symbol = std::wstring(shader_src.m_entry.begin(), shader_src.m_entry.end());
            entry.m_renamed_symbol = std::wstring(renamed_symbol.begin(), renamed_symbol.end());
            // entry.m_descriptor_set_info = reflection_result.m_space_bindings;

            if (rt_lib.m_shader_srcs[i].m_shader_stage == ShaderStageEnum::RayGen)
            {
                m_raygen_renamed_symbols.push_back(entry.m_renamed_symbol);
            }
            else if (rt_lib.m_shader_srcs[i].m_shader_stage == ShaderStageEnum::Miss)
            {
                m_miss_renamed_symbols.push_back(entry.m_renamed_symbol);
            }
        }

        std::vector<HitGroupRecord> hit_group_records(rt_lib.m_hit_groups.size());
        for (size_t i = 0; i < rt_lib.m_hit_groups.size(); i++)
        {
            const auto &     hit_group        = rt_lib.m_hit_groups[i];
            HitGroupRecord & hit_shader_group = hit_group_records[i];

            // automatically generate hit_group symbol
            std::wstring symbol = L"hit_group";
            if (hit_group.m_closest_hit_id.has_value())
            {
                hit_shader_group.m_closest_hit_id = hit_group.m_closest_hit_id.value();
                symbol += L"_" + shader_entries[hit_group.m_closest_hit_id.value()].m_renamed_symbol;
            }

            if (hit_group.m_any_hit_id.has_value())
            {
                hit_shader_group.m_any_hit_id = hit_group.m_any_hit_id.value();
                symbol += L"_" + shader_entries[hit_group.m_any_hit_id.value()].m_renamed_symbol;
            }

            if (hit_group.m_intersect_id.has_value())
            {
                hit_shader_group.m_intersect_id = hit_group.m_intersect_id.value();
                symbol += L"_" + shader_entries[hit_group.m_intersect_id.value()].m_renamed_symbol;
            }

            // generate symbol
            hit_shader_group.m_symbol = symbol;

            m_hit_group_renamed_symbols.push_back(symbol);
        }

        // build local root signatures (equivalent to shaderrecord)
        {
            // TODO:: move local root signatures building process from RayTracingPipelineConfig to
            // here (pipeline config should only contains configuration)
        }

        // build global root signature
        {
            // prepare array for reflection
            std::vector<std::pair<ComPtr<IDxcBlob>, DXA_NAME::ShaderStageEnum>> shaders(
                rt_lib.m_shader_srcs.size());
            for (size_t i_shader = 0; i_shader < shaders.size(); i_shader++)
            {
                const auto & shader_src  = rt_lib.m_shader_srcs[i_shader];
                shaders[i_shader].first  = hlsl_dxil_compiler.compile_as_dxil(shader_src);
                shaders[i_shader].second = shader_src.m_shader_stage;
            }

            // reflecting as root parameters
            const auto reflection_result = dxil_reflector.reflect(shaders, rt_lib.m_shader_srcs);

            // create root signature and check for errors
            CD3DX12_ROOT_SIGNATURE_DESC root_signature_desc = {};
            root_signature_desc.Init(static_cast<UINT>(reflection_result.m_root_parameters.size()),
                                     reflection_result.m_root_parameters.data(),
                                     0,
                                     nullptr,
                                     D3D12_ROOT_SIGNATURE_FLAG_NONE);
            ComPtr<ID3DBlob> signature = nullptr;
            ComPtr<ID3DBlob> error     = nullptr;
            HRESULT          root_description_create_result =
                D3D12SerializeRootSignature(&root_signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
            if (error != nullptr)
            {
                Logger::Critical<true>(__FUNCTION__ " ", static_cast<char *>(error->GetBufferPointer()));
            }
            DXCK(root_description_create_result);
            DXCK(device->m_dx_device->CreateRootSignature(0,
                                                          signature->GetBufferPointer(),
                                                          signature->GetBufferSize(),
                                                          IID_PPV_ARGS(&m_dx_global_root_signature)));

            m_descriptor_set_info = reflection_result.m_space_bindings;
        }

        init_pso(device, rt_lib, shader_entries, hit_group_records, attribute_size, payload_size, recursion_depth, name);

        if (device->enable_debug())
        {
            if (!name.empty())
            {
                std::wstring wname(name.begin(), name.end());
                m_dx_rt_pso->SetName(wname.c_str());
            }
        }
    }

    void
    init_pso(const Device *                      device,
             const RayTracingPipelineConfig &    rt_lib,
             const std::vector<ShaderEntry> &    shader_entries,
             const std::vector<HitGroupRecord> & hit_group_records,
             const size_t                        attribute_size,
             const size_t                        payload_size,
             const size_t                        recursion_depth,
             const std::string &                 name)
    {

        auto num_shader_entries = [&](const ShaderStageEnum stage) -> size_t {
            size_t result = 0;
            for (auto & src : rt_lib.m_shader_srcs)
            {
                if (src.m_shader_stage == stage)
                {
                    result += 1;
                }
            }
            return result;
        };

        auto num_max_inputs = [&](const ShaderStageEnum stage) -> size_t {
            size_t max_inputs = 0;
            for (size_t i = 0; i < shader_entries.size(); i++)
            {
                const auto & src   = rt_lib.m_shader_srcs[i];
                const auto & entry = shader_entries[i];
                if (src.m_shader_stage == stage)
                {
                    max_inputs = std::max(entry.m_num_root_parameters, max_inputs);
                }
            }
            return max_inputs;
        };

        auto get_record_size = [&](const ShaderStageEnum stage) -> size_t {
            size_t result = 0;
            result += D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
            result += num_max_inputs(stage) * 8; // 8 bytes per input
            result = round_up(result, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
            return result;
        };

        const size_t num_raygens    = num_shader_entries(ShaderStageEnum::RayGen);
        const size_t num_misses     = num_shader_entries(ShaderStageEnum::Miss);
        const size_t num_shaders    = shader_entries.size();
        const size_t num_hit_groups = hit_group_records.size();
        const size_t num_records    = num_raygens + num_misses + num_hit_groups;

        // subobject for setting up pso
        std::vector<D3D12_STATE_SUBOBJECT>   subobjects(num_shaders      // export dxil library
                                                      + num_hit_groups // export hit group
                                                      + num_records * 2 // local root signatures and its association
                                                      + 2   // shader config and its association
                                                      + 1   // global root signature
                                                      + 1); // pipeline config
        size_t                               num_subobjects = 0;
        std::vector<D3D12_EXPORT_DESC>       dxil_export_desc(num_shaders);
        size_t                               num_dxil_export_desc = 0;
        std::vector<D3D12_DXIL_LIBRARY_DESC> dxil_library_descs(num_shaders);
        size_t                               num_dxil_library_descs = 0;
        std::vector<D3D12_HIT_GROUP_DESC>    hit_group_descs(num_hit_groups);
        size_t                               num_hit_group_descs = 0;
        std::vector<std::array<LPCWSTR, 1>>  local_root_signature_symbols_descs(num_records);
        size_t                               num_local_root_signature_symbols_descs = 0;
        std::vector<LPCWSTR>                 payload_assoc_symbols(num_records);
        size_t                               num_payload_assoc_symbols = 0;
        std::vector<D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION> local_root_signature_export_assocs(num_records);
        size_t num_local_root_signature_export_assocs = 0;

        // all dxil library
        for (const auto & entry : shader_entries)
        {
            // describe the dxil library entry point
            D3D12_EXPORT_DESC * export_desc = &dxil_export_desc[num_dxil_export_desc++];
            export_desc->Flags              = D3D12_EXPORT_FLAG_NONE;
            export_desc->Name               = entry.m_renamed_symbol.c_str();
            export_desc->ExportToRename     = entry.m_entry_symbol.c_str();

            // library description
            D3D12_DXIL_LIBRARY_DESC * dxil_library_desc = &dxil_library_descs[num_dxil_library_descs++];
            dxil_library_desc->DXILLibrary.BytecodeLength = entry.m_compiled_shader_blob->GetBufferSize();
            dxil_library_desc->DXILLibrary.pShaderBytecode = entry.m_compiled_shader_blob->GetBufferPointer();
            dxil_library_desc->NumExports                  = 1;
            dxil_library_desc->pExports                    = export_desc;

            // SUBOBJECT - dxil library
            D3D12_STATE_SUBOBJECT * dxil_library = &subobjects[num_subobjects++];
            dxil_library->Type                   = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
            dxil_library->pDesc                  = dxil_library_desc;
        }

        // all hit_group
        for (const auto & hit_group : hit_group_records)
        {
            // fetch all entries
            const auto * closest_hit_entry = hit_group.m_closest_hit_id.has_value()
                                                 ? &shader_entries[hit_group.m_closest_hit_id.value()]
                                                 : nullptr;
            const auto * any_hit_entry =
                hit_group.m_any_hit_id.has_value() ? &shader_entries[hit_group.m_any_hit_id.value()] : nullptr;
            const auto * intersect_entry = hit_group.m_intersect_id.has_value()
                                               ? &shader_entries[hit_group.m_intersect_id.value()]
                                               : nullptr;

            // hit group
            D3D12_HIT_GROUP_DESC * hit_group_desc    = &hit_group_descs[num_hit_group_descs++];
            hit_group_desc->ClosestHitShaderImport   = closest_hit_entry->m_renamed_symbol.c_str();
            hit_group_desc->AnyHitShaderImport       = nullptr;
            hit_group_desc->IntersectionShaderImport = nullptr;

            assert(rt_lib.m_shader_srcs[hit_group.m_closest_hit_id.value()].m_shader_stage ==
                   ShaderStageEnum::ClosestHit);
            if (any_hit_entry)
            {
                hit_group_desc->AnyHitShaderImport = any_hit_entry->m_renamed_symbol.c_str();
                assert(rt_lib.m_shader_srcs[hit_group.m_any_hit_id.value()].m_shader_stage ==
                       ShaderStageEnum::AnyHit);
            }
            if (intersect_entry)
            {
                hit_group_desc->IntersectionShaderImport = intersect_entry->m_renamed_symbol.c_str();
                assert(rt_lib.m_shader_srcs[hit_group.m_intersect_id.value()].m_shader_stage ==
                       ShaderStageEnum::Intersection);
            }
            hit_group_desc->Type           = D3D12_HIT_GROUP_TYPE_TRIANGLES;
            hit_group_desc->HitGroupExport = hit_group.m_symbol.c_str();

            // SUBOBJECT - hit group
            D3D12_STATE_SUBOBJECT * hit_group_sub = &subobjects[num_subobjects++];
            hit_group_sub->Type                   = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
            hit_group_sub->pDesc                  = hit_group_desc;
        }

        // local root signature for raygen, miss and hit_group
        for (size_t i = 0; i < shader_entries.size() + hit_group_records.size(); i++)
        {
            ID3D12RootSignature * const * root_signature_ptr = nullptr;
            LPCWSTR                       symbol             = nullptr;

            if (i < shader_entries.size())
            {
                // entries
                const auto & entry = shader_entries[i];
                const auto & src   = rt_lib.m_shader_srcs[i];
                if (src.m_shader_stage == ShaderStageEnum::RayGen || src.m_shader_stage == ShaderStageEnum::Miss)
                {
                    if (entry.m_local_root_signature == nullptr)
                    {
                        root_signature_ptr = device->m_dx_empty_local_root_signature.GetAddressOf();
                    }
                    else
                    {
                        root_signature_ptr = entry.m_local_root_signature.GetAddressOf();
                    }
                    symbol = entry.m_renamed_symbol.c_str();
                }
            }
            else
            {
                // hit_group
                const auto & hit_group = hit_group_records[i - shader_entries.size()];
                if (shader_entries[hit_group.m_closest_hit_id.value()].m_local_root_signature == nullptr)
                {
                    root_signature_ptr = device->m_dx_empty_local_root_signature.GetAddressOf();
                }
                else
                {
                    root_signature_ptr =
                        shader_entries[hit_group.m_closest_hit_id.value()].m_local_root_signature.GetAddressOf();
                }
                symbol = hit_group.m_symbol.c_str();
            }

            if (symbol)
            {
                // SUBOBJECT - local root signature
                D3D12_STATE_SUBOBJECT * local_root_signature = &subobjects[num_subobjects++];
                local_root_signature->Type  = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
                local_root_signature->pDesc = root_signature_ptr;

                // local root signature assoc
                std::array<LPCWSTR, 1> * root_signature_symbol =
                    &local_root_signature_symbols_descs[num_local_root_signature_symbols_descs++];
                root_signature_symbol->at(0) = symbol;

                // subobject assocs
                D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION * root_signature_assoc_export =
                    &local_root_signature_export_assocs[num_local_root_signature_export_assocs++];
                root_signature_assoc_export->pExports              = root_signature_symbol->data();
                root_signature_assoc_export->NumExports            = 1;
                root_signature_assoc_export->pSubobjectToAssociate = local_root_signature;

                // SUBOBJECT - local root signature assoc
                D3D12_STATE_SUBOBJECT * root_signature_assoc = &subobjects[num_subobjects++];
                root_signature_assoc->Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
                root_signature_assoc->pDesc = root_signature_assoc_export;
            }
        }

        // SUBOBJECT - global root signature assoc
        D3D12_STATE_SUBOBJECT * global_root_signature_assoc = &subobjects[num_subobjects++];
        global_root_signature_assoc->Type  = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
        global_root_signature_assoc->pDesc = m_dx_global_root_signature.GetAddressOf();

        // symbols for shader payload
        for (size_t i = 0; i < shader_entries.size() + hit_group_records.size(); i++)
        {
            if (i < shader_entries.size())
            {
                // entries
                const auto & entry = shader_entries[i];
                const auto & src   = rt_lib.m_shader_srcs[i];
                if (src.m_shader_stage == ShaderStageEnum::RayGen || src.m_shader_stage == ShaderStageEnum::Miss)
                {
                    payload_assoc_symbols[num_payload_assoc_symbols++] = entry.m_renamed_symbol.c_str();
                }
            }
            else
            {
                const auto & hit_group = hit_group_records[i - shader_entries.size()];
                payload_assoc_symbols[num_payload_assoc_symbols++] = hit_group.m_symbol.c_str();
            }
        }

        // payload shader config
        D3D12_RAYTRACING_SHADER_CONFIG shader_config_desc = {};
        shader_config_desc.MaxAttributeSizeInBytes        = static_cast<UINT>(attribute_size);
        shader_config_desc.MaxPayloadSizeInBytes          = static_cast<UINT>(payload_size);
        assert(shader_config_desc.MaxAttributeSizeInBytes <= D3D12_RAYTRACING_MAX_ATTRIBUTE_SIZE_IN_BYTES);

        // SUBOBJECT - payload shader config
        D3D12_STATE_SUBOBJECT * shader_config = &subobjects[num_subobjects++];
        shader_config->Type                   = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
        shader_config->pDesc                  = &shader_config_desc;

        // associate payload with raygen, miss and hit_groups
        D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION shader_payload_assoc_exports = {};
        shader_payload_assoc_exports.NumExports = static_cast<UINT>(payload_assoc_symbols.size());
        shader_payload_assoc_exports.pExports   = payload_assoc_symbols.data();
        shader_payload_assoc_exports.pSubobjectToAssociate = shader_config;

        // SUBOBJECT - shader payload
        D3D12_STATE_SUBOBJECT * shader_payload_assoc = &subobjects[num_subobjects++];
        shader_payload_assoc->Type  = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
        shader_payload_assoc->pDesc = &shader_payload_assoc_exports;

        // pipeline config
        D3D12_RAYTRACING_PIPELINE_CONFIG pipeline_config = {};
        pipeline_config.MaxTraceRecursionDepth           = static_cast<UINT>(recursion_depth);

        // SUBOBJECT - pipeline config
        D3D12_STATE_SUBOBJECT * pipelineConfigObject = &subobjects[num_subobjects++];
        pipelineConfigObject->Type  = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
        pipelineConfigObject->pDesc = &pipeline_config;

        // preallocated subobjects must match subobject used
        // assert(num_subobjects == subobjects.size());
        assert(num_dxil_export_desc == dxil_export_desc.size());
        assert(num_dxil_library_descs == dxil_library_descs.size());
        assert(num_hit_group_descs == hit_group_descs.size());
        assert(num_payload_assoc_symbols == payload_assoc_symbols.size());
        // assert(num_local_root_signature_symbols_descs == local_root_signature_symbols_descs.size());
        // assert(num_local_root_signature_export_assocs == local_root_signature_export_assocs.size());

        // state objects description
        D3D12_STATE_OBJECT_DESC pipeline_desc = {};
        pipeline_desc.Type                    = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
        pipeline_desc.NumSubobjects           = static_cast<UINT>(num_subobjects);
        pipeline_desc.pSubobjects             = subobjects.data();

        // create state object
        DXCK(device->m_dx_device->CreateStateObject(&pipeline_desc, IID_PPV_ARGS(&m_dx_rt_pso)));

        DXCK(m_dx_rt_pso->QueryInterface(IID_PPV_ARGS(&m_dx_rt_pso_props)));

        const size_t raygen_record_size    = get_record_size(ShaderStageEnum::RayGen);
        const size_t miss_record_size      = get_record_size(ShaderStageEnum::Miss);
        const size_t hit_group_record_size = get_record_size(ShaderStageEnum::ClosestHit);

        m_raygen_record_size    = raygen_record_size;
        m_miss_record_size      = miss_record_size;
        m_hit_group_record_size = hit_group_record_size;
        m_num_raygens           = num_shader_entries(ShaderStageEnum::RayGen);
        m_num_misses            = num_shader_entries(ShaderStageEnum::Miss);
        m_num_hit_groups        = num_hit_groups;

        device->name_dx_object(m_dx_rt_pso, name);
    }
};

struct RayTracingShaderTable
{
    D3D12MAHandle<D3D12MA::Allocation> m_shader_table_buffer   = nullptr;
    D3D12_DISPATCH_RAYS_DESC           m_dx_dispatch_rays_desc = {};

    RayTracingShaderTable() {}

    RayTracingShaderTable(const Device *             device,
                          const RayTracingPipeline & pipeline,
                          const std::string &        name,
                          // TODO:: these gpu descriptor handle should be set in a different function
                          const D3D12_GPU_DESCRIPTOR_HANDLE raygen_handle    = { 0 },
                          const D3D12_GPU_DESCRIPTOR_HANDLE miss_handle      = { 0 },
                          const D3D12_GPU_DESCRIPTOR_HANDLE hit_group_handle = { 0 })
    {
        auto & desc = m_dx_dispatch_rays_desc;

        const size_t raygen_size   = pipeline.m_raygen_record_size * pipeline.m_num_raygens;
        const size_t miss_size     = pipeline.m_miss_record_size * pipeline.m_num_misses;
        const size_t hitgroup_size = pipeline.m_hit_group_record_size * pipeline.m_num_hit_groups;
        const size_t rounded_raygen_size = round_up(raygen_size, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
        const size_t rounded_miss_size = round_up(miss_size, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
        const size_t rounded_hitgroup_size =
            round_up(hitgroup_size, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
        const size_t shader_table_size = rounded_raygen_size + rounded_miss_size + rounded_hitgroup_size;

        // allocate resource
        m_shader_table_buffer = [&]() {
            // Create shader table buffer
            D3D12MA::ALLOCATION_DESC alloc_desc = {};
            alloc_desc.Flags                    = D3D12MA::ALLOCATION_FLAG_COMMITTED;
            alloc_desc.HeapType                 = D3D12_HEAP_TYPE_UPLOAD;

            ID3D12Resource *      resource;
            D3D12MA::Allocation * allocation = nullptr;
            CD3DX12_RESOURCE_DESC buffer_desc =
                CD3DX12_RESOURCE_DESC::Buffer(shader_table_size, D3D12_RESOURCE_FLAG_NONE);
            DXCK(device->m_d3d12ma->CreateResource(&alloc_desc,
                                                   &buffer_desc,
                                                   D3D12_RESOURCE_STATE_GENERIC_READ,
                                                   nullptr,
                                                   &allocation,
                                                   IID_PPV_ARGS(&resource)));
            return D3D12MAHandle<D3D12MA::Allocation>(allocation);
        }();

        // write sbt
        uint8_t * start_address = nullptr;
        DXCK(m_shader_table_buffer->GetResource()->Map(0, nullptr, (void **)&start_address));

        uint8_t * sbtp = nullptr;
        sbtp           = start_address;
        for (const auto & symbol : pipeline.m_raygen_renamed_symbols)
        {
            std::memcpy(sbtp,
                        pipeline.m_dx_rt_pso_props->GetShaderIdentifier(symbol.c_str()),
                        D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
            if (pipeline.m_raygen_record_size > D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES)
            {
                // TODO:: copy multiple handles to support multiple uniforms in local root signatures
                std::memcpy(sbtp + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, &raygen_handle, sizeof(raygen_handle));
            }
            sbtp += pipeline.m_raygen_record_size;
        }

        sbtp = start_address + rounded_raygen_size;
        for (const auto & symbol : pipeline.m_miss_renamed_symbols)
        {
            std::memcpy(sbtp,
                        pipeline.m_dx_rt_pso_props->GetShaderIdentifier(symbol.c_str()),
                        D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
            if (pipeline.m_miss_record_size > D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES)
            {
                // TODO:: copy multiple handles to support multiple uniforms in local root signatures
                std::memcpy(sbtp + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, &miss_handle, sizeof(miss_handle));
            }
            sbtp += pipeline.m_miss_record_size;
        }

        sbtp = start_address + rounded_raygen_size + rounded_miss_size;
        for (const auto & symbol : pipeline.m_hit_group_renamed_symbols)
        {
            std::memcpy(sbtp,
                        pipeline.m_dx_rt_pso_props->GetShaderIdentifier(symbol.c_str()),
                        D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
            if (pipeline.m_hit_group_record_size > D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES)
            {
                // TODO:: copy multiple handles to support multiple uniforms in local root signatures
                std::memcpy(sbtp + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, &hit_group_handle, sizeof(hit_group_handle));
            }
            sbtp += pipeline.m_hit_group_record_size;
        }

        m_shader_table_buffer->GetResource()->Unmap(0, nullptr);

        // raygen
        desc.RayGenerationShaderRecord.StartAddress =
            m_shader_table_buffer->GetResource()->GetGPUVirtualAddress();
        desc.RayGenerationShaderRecord.SizeInBytes = raygen_size;

        // miss
        desc.MissShaderTable.StartAddress = desc.RayGenerationShaderRecord.StartAddress + rounded_raygen_size;
        desc.MissShaderTable.StrideInBytes = pipeline.m_miss_record_size;
        desc.MissShaderTable.SizeInBytes   = miss_size;

        // hitgroup
        desc.HitGroupTable.StartAddress =
            desc.RayGenerationShaderRecord.StartAddress + rounded_raygen_size + rounded_miss_size;
        desc.HitGroupTable.StrideInBytes = pipeline.m_hit_group_record_size;
        desc.HitGroupTable.SizeInBytes   = hitgroup_size;

        // callable
        desc.CallableShaderTable.StartAddress  = 0;
        desc.CallableShaderTable.SizeInBytes   = 0;
        desc.CallableShaderTable.StrideInBytes = 0;

        // set debug name
        if (device->enable_debug())
        {
            if (!name.empty())
            {
                const std::wstring wname(name.begin(), name.end());
                m_shader_table_buffer->GetResource()->SetName(wname.c_str());
            }
        }
    }
};
} // namespace DXA_NAME
