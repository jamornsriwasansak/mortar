#pragma once

#include "dxaframebufferbinding.h"
#include "dxarasterpipeline.h"
#include "dxatexture.h"

#include "../shaderintrospect/hlsldxccompiler.h"
#include "../sharedsignature/sharedvertex.h"

/*
namespace Dxa
{
struct MaterialSystem
{
    ComPtr<ID3D12RootSignature> m_dx_root_signature = nullptr;
    std::vector<RasterPipeline> m_material_raster_pipelines;
    std::vector<Texture>        m_textures;
    FramebufferBindings *      m_framebuffer_bindings;
    D3D12_CPU_DESCRIPTOR_HANDLE m_texture_cpu_handle;
    D3D12_GPU_DESCRIPTOR_HANDLE m_texture_gpu_handle;
    D3D12_GPU_DESCRIPTOR_HANDLE m_sampler_gpu_handle;
    size_t                      m_num_textures;

    MaterialSystem(FramebufferBindings * framebuffer_bindings, const size_t num_textures, const std::string & name)
    : m_dx_root_signature(create_root_signature(num_textures, name)),
      m_framebuffer_bindings(framebuffer_bindings),
      m_num_textures(num_textures)
    {
        //init_default_sampler();
        //init_texture_handle(num_textures);
    }

    size_t
    add_pipeline(const std::vector<ShaderSrc> & shader_srcs)
    {
        m_material_raster_pipelines.push_back(
            RasterPipeline(shader_srcs, m_dx_root_signature, *m_framebuffer_bindings));
        return m_material_raster_pipelines.size() - 1;
    }

    void
    set_texture(const size_t index, const Texture & texture)
    {
        assert(index < m_num_textures);

        // offset
        size_t heap_increment_size =
            DDev(0)->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        CD3DX12_CPU_DESCRIPTOR_HANDLE handle(m_texture_cpu_handle, static_cast<INT>(index * heap_increment_size));

        // create shader resource view
        D3D12_SHADER_RESOURCE_VIEW_DESC material_srv_desc = {};
        material_srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        material_srv_desc.Format                  = texture.m_dx_format;
        material_srv_desc.ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D;
        material_srv_desc.Texture2D.MipLevels     = 1;
        Dev(0)->m_dx_device->CreateShaderResourceView(texture.m_dx_resource, &material_srv_desc, handle);
    }

private:
    ComPtr<ID3D12RootSignature>
    create_root_signature(const size_t num_textures, const std::string & name)
    {
        // material root signature
        ComPtr<ID3D12RootSignature> root_signature = nullptr;

        // init descriptor ranges
        CD3DX12_DESCRIPTOR_RANGE descriptor_ranges[2];
        descriptor_ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, static_cast<UINT>(num_textures), 0, 0);
        descriptor_ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0, 0);

        // given descriptor ranges, init root parameters
        // slot 0 = necessary matrices vertex shader (transforms, etc..)
        // slot 1 = cbv256 (contains material information. totally flexible)
        // slot 2 = bindless texture
        // slot 3 = sampler
        CD3DX12_ROOT_PARAMETER root_parameters[4];
        root_parameters[0].InitAsConstants(sizeof(HlslTransform) / 4, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
        root_parameters[1].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
        root_parameters[2].InitAsDescriptorTable(1, &descriptor_ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);
        root_parameters[3].InitAsDescriptorTable(1, &descriptor_ranges[1], D3D12_SHADER_VISIBILITY_PIXEL);

        // given root parameters, init root signature description
        CD3DX12_ROOT_SIGNATURE_DESC root_signature_desc;
        root_signature_desc.Init(_countof(root_parameters), root_parameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        // create signature and error blob
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
        if constexpr (GraphicsApiDebug)
        {
            if (!name.empty())
            {
                std::wstring wname(name.begin(), name.end());
                root_signature->SetName(wname.c_str());
            }
        }

        return root_signature;
    }

    void
    init_texture_handle(const size_t num_textures)
    {
        // create srv handle
        m_texture_cpu_handle =
            CD3DX12_CPU_DESCRIPTOR_HANDLE(Dev(0)->m_dx_cbv_srv_uav_descriptor_heap->GetCPUDescriptorHandleForHeapStart(),
                                          Dev(0)->m_dx_cbv_srv_uav_heap_offset);
        m_texture_gpu_handle =
            CD3DX12_GPU_DESCRIPTOR_HANDLE(Dev(0)->m_dx_cbv_srv_uav_descriptor_heap->GetGPUDescriptorHandleForHeapStart(),
                                          Dev(0)->m_dx_cbv_srv_uav_heap_offset);

        // offset srv uav heap offset
        Dev(0)->m_dx_cbv_srv_uav_heap_offset +=
            DDev(0)->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) *
            static_cast<INT>(num_textures);
    }

    void
    init_default_sampler()
    {
        // create sampler handle
        m_sampler_gpu_handle =
            CD3DX12_GPU_DESCRIPTOR_HANDLE(Dev(0)->m_dx_sampler_heap->GetGPUDescriptorHandleForHeapStart(),
                                          Dev(0)->m_dx_sampler_heap_offset);
        CD3DX12_CPU_DESCRIPTOR_HANDLE sampler_cpu_handle(Dev(0)->m_dx_sampler_heap->GetCPUDescriptorHandleForHeapStart(),
                                                         Dev(0)->m_dx_sampler_heap_offset);

        // sampler description
        D3D12_SAMPLER_DESC sampler_desc = {};
        sampler_desc.Filter             = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        sampler_desc.AddressU           = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler_desc.AddressV           = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler_desc.AddressW           = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler_desc.MinLOD             = 0;
        sampler_desc.MaxLOD             = D3D12_FLOAT32_MAX;
        sampler_desc.MipLODBias         = 0.0f;
        sampler_desc.MaxAnisotropy      = 1;
        sampler_desc.ComparisonFunc     = D3D12_COMPARISON_FUNC_ALWAYS;
        DDev(0)->CreateSampler(&sampler_desc, sampler_cpu_handle);

        // offset main sampler heap
        Dev(0)->m_dx_sampler_heap_offset +=
            DDev(0)->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
    }
};
} // namespace Dxa
*/