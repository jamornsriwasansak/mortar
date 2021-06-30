#pragma once

#include "dxacommon.h"
#include "dxadevice.h"
#include "dxaswapchain.h"
#include "dxatexture.h"

#include "backends/imgui_impl_dx12.h"
#include "backends/imgui_impl_glfw.h"
#include "imgui.h"
#include <d3d12.h>

namespace Dxa
{
struct ImGuiRenderPass
{
    ComPtr<ID3D12DescriptorHeap> m_dx_descriptor_heap = nullptr;

    ImGuiRenderPass() {}

    ImGuiRenderPass(const Device * device, const Window & window, const Swapchain & swapchain, const size_t num_flights)
    {
        // create descriptor heap for imgui font
        D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {};
        heap_desc.NumDescriptors             = static_cast<UINT>(500);
        heap_desc.Flags                      = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        heap_desc.Type                       = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        DXCK(device->m_dx_device->CreateDescriptorHeap(&heap_desc,
                                                       IID_PPV_ARGS(m_dx_descriptor_heap.GetAddressOf())));

        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO & io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        (void)io;

        // Setup Dear ImGui style
        //ImGui::StyleColorsDark();
        ImGui::StyleColorsClassic();

        // Setup Platform/Renderer backends
        ImGui_ImplGlfw_InitForOther(window.m_glfw_window, true);
        ImGui_ImplDX12_Init(device->m_dx_device.Get(),
                            swapchain.m_num_images,
                            swapchain.m_dx_format,
                            m_dx_descriptor_heap.Get(),
                            m_dx_descriptor_heap->GetCPUDescriptorHandleForHeapStart(),
                            m_dx_descriptor_heap->GetGPUDescriptorHandleForHeapStart());
    }

    void
    new_frame() const
    {
        // Start the Dear ImGui frame
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void
    end_frame() const
    {
        ImGui::EndFrame();
    }

    void
    shut_down() const
    {
        // Cleanup
        ImGui_ImplDX12_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
};
} // namespace Dxa