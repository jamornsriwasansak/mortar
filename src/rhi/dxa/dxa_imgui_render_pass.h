#pragma once

#include "dxa_common.h"
#ifdef USE_DXA

    #include "dxa_device.h"
    #include "dxa_swapchain.h"
    #include "dxa_texture.h"

    #include "backends/imgui_impl_dx12.h"
    #include "backends/imgui_impl_glfw.h"
    #include "imgui.h"
    #include <d3d12.h>

namespace DXA_NAME
{
struct ImGuiRenderPass
{
    ComPtr<ID3D12DescriptorHeap> m_dx_descriptor_heap = nullptr;

    ImGuiRenderPass() {}

    ImGuiRenderPass(const std::string & name, const Device & device, const Window & window, const Swapchain & swapchain)
    {
        // create descriptor heap for imgui font
        D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {};
        heap_desc.NumDescriptors             = static_cast<UINT>(5);
        heap_desc.Flags                      = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        heap_desc.Type                       = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        DXCK(device.m_dx_device->CreateDescriptorHeap(&heap_desc,
                                                      IID_PPV_ARGS(m_dx_descriptor_heap.GetAddressOf())));
        device.name_dx_object(m_dx_descriptor_heap, name + "_descriptor_heap");

        // Setup Platform/Renderer backends
        ImGui_ImplGlfw_InitForOther(window.m_glfw_window, true);
        ImGui_ImplDX12_Init(device.m_dx_device.Get(),
                            swapchain.m_num_images,
                            swapchain.m_dx_format,
                            m_dx_descriptor_heap.Get(),
                            m_dx_descriptor_heap->GetCPUDescriptorHandleForHeapStart(),
                            m_dx_descriptor_heap->GetGPUDescriptorHandleForHeapStart());
    }

    void
    init_or_resize_framebuffer([[maybe_unused]] const Device & device, [[maybe_unused]] const Swapchain & swapchain)
    {
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
        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
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
} // namespace DXA_NAME
#endif