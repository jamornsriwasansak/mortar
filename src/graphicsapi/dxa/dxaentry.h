#pragma once
#include "dxacommon.h"

namespace DXA_NAME
{
struct PhysicalDevice
{
    ComPtr<IDXGIAdapter4> m_dx_adapter   = nullptr;
    bool                  m_enable_debug = false;

    PhysicalDevice() {}

    PhysicalDevice(ComPtr<IDXGIAdapter4> & adapter, bool enable_debug)
    : m_dx_adapter(adapter), m_enable_debug(enable_debug)
    {
    }
};

struct Entry
{
    ComPtr<IDXGIFactory4> m_dx_factory;
    bool                  m_debug;

    Entry() {}

    Entry([[maybe_unused]] const Window & window, const bool debug) : m_debug(debug)
    {
        UINT factory_flags = 0;
        if (debug)
        {
            enable_debug();
            factory_flags = DXGI_CREATE_FACTORY_DEBUG;
        }
        DXCK(CreateDXGIFactory2(factory_flags, IID_PPV_ARGS(&m_dx_factory)));
    }

    std::vector<PhysicalDevice>
    get_graphics_devices()
    {
        std::vector<PhysicalDevice> result;
        ComPtr<IDXGIAdapter1>       dx_adapter1;

        // iterate over all possible adapters
        for (UINT i = 0; m_dx_factory->EnumAdapters1(i, dx_adapter1.GetAddressOf()) != DXGI_ERROR_NOT_FOUND; i++)
        {
            // get adapter description
            DXGI_ADAPTER_DESC1 dx_adapter_desc;
            dx_adapter1->GetDesc1(&dx_adapter_desc);
            if ((dx_adapter_desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
                SUCCEEDED(D3D12CreateDevice(dx_adapter1.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)))
            {
                // convert dx_adapter1 to dx_adapter4
                ComPtr<IDXGIAdapter4> dx_adapter4;
                if (SUCCEEDED(dx_adapter1.As(&dx_adapter4)))
                {
                    result.emplace_back(dx_adapter4, m_debug);
                }
            }
        }
        return result;
    }

private:
    void
    enable_debug()
    {
        ComPtr<ID3D12Debug>  debug0;
        ComPtr<ID3D12Debug1> debug1;

#ifdef USE_NSIGHT_AFTERMATH
        Logger::Warn(__FUNCTION__,
                     " D3D12GetDebugInterface is disabled due to conflict with Nsight Aftermath");
#else
        // enable debug layer
        DXCK(D3D12GetDebugInterface(IID_PPV_ARGS(&debug0)));
        debug0->EnableDebugLayer();
#endif

        // enable gpu based validation
        // DXCK(debug0->QueryInterface(IID_PPV_ARGS(&debug1)));
        // debug1->SetEnableGPUBasedValidation(true);
    }
};
} // namespace DXA_NAME