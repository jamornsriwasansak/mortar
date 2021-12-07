#pragma once

#include "dxacommon.h"
#ifdef USE_DXA

    #include "dxadevice.h"
    #include "dxasemaphore.h"

namespace DXA_NAME
{
struct Swapchain
{
    static constexpr size_t m_num_images = 3;

    ComPtr<IDXGISwapChain4>             m_dx_swapchain;
    std::vector<ComPtr<ID3D12Resource>> m_dx_swapchain_resource_pointers;
    DXGI_FORMAT                         m_dx_format;
    size_t                              m_image_index;
    int2                                m_resolution;
    std::string                         m_name;

    Swapchain(const std::string & name, const Device & device, const Window & window)
    : m_dx_swapchain(construct_dx_swapchain(device, window, DXGI_FORMAT_R8G8B8A8_UNORM)),
      m_dx_swapchain_resource_pointers(construct_swapchain_resource_pointers(name, m_dx_swapchain.Get(), device)),
      m_dx_format(DXGI_FORMAT_R8G8B8A8_UNORM),
      m_image_index(m_dx_swapchain->GetCurrentBackBufferIndex()),
      m_resolution(window.get_resolution()),
      m_name(name)
    {
    }

    static ComPtr<IDXGISwapChain4>
    construct_dx_swapchain(const Device & device, const Window & window, const DXGI_FORMAT dx_format)
    {
        // It is recommended to always allow tearing if tearing support is available.
        DXGI_SWAP_CHAIN_DESC1 swapchain_desc = {};
        int2                  resolution     = window.get_resolution();
        swapchain_desc.Width                 = resolution.x;
        swapchain_desc.Height                = resolution.y;
        swapchain_desc.Format                = dx_format;
        swapchain_desc.Stereo                = FALSE;
        swapchain_desc.SampleDesc            = { 1, 0 };
        swapchain_desc.BufferUsage           = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapchain_desc.BufferCount           = m_num_images;
        swapchain_desc.Scaling               = DXGI_SCALING_STRETCH;
        swapchain_desc.SwapEffect            = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapchain_desc.AlphaMode             = DXGI_ALPHA_MODE_UNSPECIFIED;
        swapchain_desc.Flags = check_tearing_support() ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
        IDXGISwapChain1 *       swapchain  = nullptr;
        ComPtr<IDXGIFactory4>   factory4   = nullptr;
        ComPtr<IDXGISwapChain4> swapchain4 = nullptr;
        DXCK(CreateDXGIFactory1(IID_PPV_ARGS(&factory4)));
        factory4->CreateSwapChainForHwnd(device.m_dx_direct_queue.Get(),
                                         window.get_hwnd(),
                                         &swapchain_desc,
                                         nullptr,
                                         nullptr,
                                         &swapchain);
        swapchain->QueryInterface(swapchain4.GetAddressOf());
        swapchain->Release();
        return swapchain4;
    }

    static std::vector<ComPtr<ID3D12Resource>>
    construct_swapchain_resource_pointers(const std::string & name, IDXGISwapChain4 * swapchain, const Device & device)
    {
        std::vector<ComPtr<ID3D12Resource>> result;
        result.resize(m_num_images);
        for (uint32_t i_frame = 0; i_frame < m_num_images; i_frame++)
        {
            // get buffer into render target
            DXCK(swapchain->GetBuffer(i_frame, IID_PPV_ARGS(&result[i_frame])));

            // name the buffer
            if (!name.empty())
            {
                device.name_dx_object(result[i_frame], name + "_" + std::to_string(i_frame));
            }
        }
        return result;
    }

    void
    update_image_index([[maybe_unused]] const Semaphore & semaphore)
    {
        // present in dx12 automatically queue the next frame so wait semaphore is never been used.
        m_image_index = m_dx_swapchain->GetCurrentBackBufferIndex();
    }

    bool
    present([[maybe_unused]] const Semaphore & wait_semaphore)
    {
        // present in dx12 automatically queue the next frame so wait semaphore is never been used.
        // TODO:: allow use of vsync through swapchain
        // TODO:: allow use of tearing support
        const bool use_vsync             = false;
        const bool use_tearing_supported = true;
        UINT       sync_interval         = use_vsync ? 1 : 0;
        UINT present_flags = use_tearing_supported && !use_vsync ? DXGI_PRESENT_ALLOW_TEARING : 0;
        DXCK(m_dx_swapchain->Present(sync_interval, present_flags));
        return true;
    }

    void
    resize_to_window(const Device & device, const Window & window)
    {
        // resize the swapchain
        DXCK(m_dx_swapchain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_R8G8B8A8_UNORM, check_tearing_support() ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0));

        // reinit swapchain resources
        m_dx_swapchain_resource_pointers =
            construct_swapchain_resource_pointers(m_name, m_dx_swapchain.Get(), device);

        // set new resolution
        m_resolution = window.get_resolution();
    }

private:
    static bool
    check_tearing_support()
    {
        BOOL allowTearing = FALSE;

        ComPtr<IDXGIFactory4> factory4;
        if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory4))))
        {
            ComPtr<IDXGIFactory5> factory5;
            if (SUCCEEDED(factory4.As(&factory5)))
            {
                if (FAILED(factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                                                         &allowTearing,
                                                         sizeof(allowTearing))))
                {
                    allowTearing = FALSE;
                }
            }
        }
        return allowTearing == TRUE;
    }
};
} // namespace DXA_NAME
#endif