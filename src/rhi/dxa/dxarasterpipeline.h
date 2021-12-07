#pragma once

#include "dxacommon.h"
#ifdef USE_DXA

    #include "../shadercompiler/hlsldxccompiler.h"
    #include "../shadercompiler/shader_binary_manager.h"
    #include "core/logger.h"
    #include "dxaframebufferbinding.h"
    #include "dxilreflection.h"


namespace DXA_NAME
{
struct RasterPipeline
{
    ComPtr<ID3D12RootSignature>   m_dx_root_signature = nullptr;
    ComPtr<ID3D12PipelineState>   m_dx_pso            = nullptr;
    D3D12_VIEWPORT                m_dx_viewport       = {};
    D3D12_RECT                    m_dx_scissor_rect   = {};
    D3D12_PRIMITIVE_TOPOLOGY_TYPE m_dx_topology_type  = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
    D3D12_PRIMITIVE_TOPOLOGY      m_dx_topology       = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
    DXGI_FORMAT                   m_dx_depth_format   = {};

    // just to improve the readability of m_descriptor_infos
    using RootSignatureIndex = size_t;
    using BindPoint          = size_t;
    using SpaceIndex         = size_t;

    std::map<std::tuple<D3D_SHADER_INPUT_TYPE, SpaceIndex, BindPoint>, DxilReflection::DescriptorInfo> m_descriptor_set_info;

    RasterPipeline(const std::string &                          name,
                   const Device &                               device,
                   const std::span<const DXA_NAME::ShaderSrc> & shader_srcs,
                   ShaderBinaryManager &                        shader_binary_manager,
                   const FramebufferBindings &                  framebuffer_bindings)
    : RasterPipeline(name, device, shader_srcs, shader_binary_manager, nullptr, framebuffer_bindings)
    {
    }

    RasterPipeline(const std::string &                          name,
                   const Device &                               device,
                   const std::span<const DXA_NAME::ShaderSrc> & shader_srcs,
                   ShaderBinaryManager &                        shader_manager,
                   const ComPtr<ID3D12RootSignature> &          root_signature,
                   const FramebufferBindings &                  framebuffer_bindings)
    : m_dx_root_signature(root_signature),
      m_dx_topology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST),
      m_dx_topology_type(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE)
    {
        init(name, device, shader_srcs, shader_manager, framebuffer_bindings);
    }

    void
    init(const std::string &                          name,
         const Device &                               device,
         const std::span<const DXA_NAME::ShaderSrc> & shader_srcs,
         ShaderBinaryManager &                        shader_manager,
         const FramebufferBindings &                  framebuffer_binding)
    {
        // compile all shader srcs
        std::vector<std::pair<ComPtr<IDxcBlob>, ShaderStageEnum>> shader_blobs(shader_srcs.size());
        {
            HlslDxcCompiler hlsl_dxil_compiler;
            for (size_t i = 0; i < shader_srcs.size(); i++)
            {
                shader_blobs[i].first  = hlsl_dxil_compiler.compile_as_dxil(shader_srcs[i]);
                shader_blobs[i].second = shader_srcs[i].m_shader_stage;
            }
        }

        // input layout description from reflection
        DxilReflection                   dxil_reflector;
        DxilReflection::ReflectionResult reflection_result = dxil_reflector.reflect(shader_blobs, shader_srcs);

        // create root signature if we don't have root signature yet
        if (m_dx_root_signature == nullptr)
        {
            CD3DX12_ROOT_SIGNATURE_DESC root_signature_desc;
            root_signature_desc.Init(static_cast<UINT>(reflection_result.m_root_parameters.size()),
                                     reflection_result.m_root_parameters.data(),
                                     0,
                                     nullptr,
                                     D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

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
            DXCK(device.m_dx_device->CreateRootSignature(0,
                                                         signature->GetBufferPointer(),
                                                         signature->GetBufferSize(),
                                                         IID_PPV_ARGS(&m_dx_root_signature)));

            // set space binding
            m_descriptor_set_info = reflection_result.m_space_bindings;
        }

        D3D12_INPUT_LAYOUT_DESC input_layout_desc = {};
        input_layout_desc.pInputElementDescs = reflection_result.m_vertex_input_element_descs.data();
        input_layout_desc.NumElements =
            static_cast<UINT>(reflection_result.m_vertex_input_element_descs.size());

        // sample description
        DXGI_SAMPLE_DESC sample_desc = {};
        sample_desc.Count            = 1;

        // make sure that cullmode is the same as opengl
        D3D12_RASTERIZER_DESC rasterize_desc = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        rasterize_desc.CullMode              = D3D12_CULL_MODE_FRONT;

        // create a pipeline state object (PSO)
        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
        pso_desc.InputLayout                        = input_layout_desc;
        pso_desc.pRootSignature                     = m_dx_root_signature.Get();
        pso_desc.PrimitiveTopologyType              = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pso_desc.SampleDesc                         = sample_desc;
        pso_desc.SampleMask                         = 0xffffffff;
        pso_desc.RasterizerState                    = rasterize_desc;
        pso_desc.BlendState                         = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        pso_desc.NumRenderTargets = static_cast<UINT>(framebuffer_binding.m_color_attachments.size());
        for (size_t i_rtv = 0; i_rtv < framebuffer_binding.m_color_attachments.size(); i_rtv++)
        {
            pso_desc.RTVFormats[i_rtv] = framebuffer_binding.m_color_attachments[i_rtv]->m_dx_format;
        }

        if (framebuffer_binding.m_depth_attachment)
        {
            pso_desc.DSVFormat         = framebuffer_binding.m_depth_attachment->m_dx_format;
            pso_desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        }

        for (size_t i = 0; i < shader_srcs.size(); i++)
        {
            D3D12_SHADER_BYTECODE shader_byte_code = {};
            shader_byte_code.BytecodeLength        = shader_blobs.at(i).first->GetBufferSize();
            shader_byte_code.pShaderBytecode       = shader_blobs.at(i).first->GetBufferPointer();

            switch (shader_srcs[i].m_shader_stage)
            {
            case ShaderStageEnum::Vertex:
                pso_desc.VS = shader_byte_code;
                break;
            case ShaderStageEnum::Fragment:
                pso_desc.PS = shader_byte_code;
                break;
            default:
                Logger::Critical<true>(__FUNCTION__ " unknown shader stage");
                break;
            }
        }

        // create the pso
        DXCK(device.m_dx_device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&m_dx_pso)));

        // Fill out the Viewport
        m_dx_viewport.TopLeftX = 0;
        m_dx_viewport.TopLeftY = 0;
        m_dx_viewport.Width =
            static_cast<FLOAT>(framebuffer_binding.m_color_attachments[0]->m_resolution.x);
        m_dx_viewport.Height =
            static_cast<FLOAT>(framebuffer_binding.m_color_attachments[0]->m_resolution.y);
        m_dx_viewport.MinDepth = 0.0f;
        m_dx_viewport.MaxDepth = 1.0f;

        // Fill out a scissor rect
        m_dx_scissor_rect.left = 0;
        m_dx_scissor_rect.top  = 0;
        m_dx_scissor_rect.right =
            static_cast<LONG>(framebuffer_binding.m_color_attachments[0]->m_resolution.x);
        m_dx_scissor_rect.bottom =
            static_cast<LONG>(framebuffer_binding.m_color_attachments[0]->m_resolution.y);

        device.name_dx_object(m_dx_pso, name);
    }
};
} // namespace DXA_NAME
#endif