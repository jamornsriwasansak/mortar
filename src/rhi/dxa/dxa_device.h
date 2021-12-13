#pragma once

#include "dxa_common.h"
#ifdef USE_DXA

    #include "dxa_entry.h"

    #ifdef USE_NSIGHT_AFTERMATH
        #include "NsightAftermathGpuCrashTracker.h"
    #endif

namespace DXA_NAME
{
struct Device
{
    ComPtr<ID3D12Device5>             m_dx_device        = nullptr;
    ComPtr<ID3D12CommandQueue>        m_dx_direct_queue  = nullptr;
    ComPtr<ID3D12CommandQueue>        m_dx_compute_queue = nullptr;
    ComPtr<ID3D12CommandQueue>        m_dx_copy_queue    = nullptr;
    D3D12MAHandle<D3D12MA::Allocator> m_d3d12ma          = nullptr;

    DescriptorHeap<DescriptorCpuHandle> m_rtv_descriptor_heap;
    DescriptorHeap<DescriptorCpuHandle> m_dsv_descriptor_heap;
    ComPtr<ID3D12RootSignature>         m_dx_empty_root_signature;
    ComPtr<ID3D12RootSignature>         m_dx_empty_local_root_signature;

    #ifdef USE_NSIGHT_AFTERMATH
    GpuCrashTracker m_nv_gpu_crash_tracker;
    #endif

    bool m_is_device_support_raytracing = false;

    Device() {}

    Device(const std::string & name, const PhysicalDevice & physical_device)
    {
        // setup debug mode
        m_debug = physical_device.m_enable_debug;

        // create device and command queues
        m_dx_device = create_device(physical_device.m_dx_adapter.Get(), physical_device.m_enable_debug);
        m_dx_direct_queue  = create_command_queue(D3D12_COMMAND_LIST_TYPE_DIRECT);
        m_dx_compute_queue = create_command_queue(D3D12_COMMAND_LIST_TYPE_COMPUTE);
        m_dx_copy_queue    = create_command_queue(D3D12_COMMAND_LIST_TYPE_COPY);
        name_dx_object(m_dx_device, name);
        name_dx_object(m_dx_direct_queue, name + "_direct_queue");
        name_dx_object(m_dx_copy_queue, name + "_copy_queue");
        name_dx_object(m_dx_compute_queue, name + "_compute_queue");

        // create dxma allocator
        D3D12MA::Allocator *    dxma_allocator = nullptr;
        D3D12MA::ALLOCATOR_DESC allocator_desc = {};
        {
            allocator_desc.pAdapter = physical_device.m_dx_adapter.Get();
            allocator_desc.pDevice  = m_dx_device.Get();
            allocator_desc.Flags    = D3D12MA::ALLOCATOR_FLAG_NONE;
        }
        D3D12MA::CreateAllocator(&allocator_desc, &dxma_allocator);
        m_d3d12ma = D3D12MAHandle<D3D12MA::Allocator>(dxma_allocator);

        // check features
        m_is_device_support_raytracing = check_raytracing_support();

        // create descriptor heap for render target view and depth stencil view
        m_dsv_descriptor_heap = DescriptorHeap<DescriptorCpuHandle>(m_dx_device.Get(),
                                                                    D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
                                                                    D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
                                                                    100);
        m_rtv_descriptor_heap = DescriptorHeap<DescriptorCpuHandle>(m_dx_device.Get(),
                                                                    D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
                                                                    D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
                                                                    100);

        // create empty global root signature
        ComPtr<ID3DBlob>          dummy_root_signature      = nullptr;
        D3D12_ROOT_SIGNATURE_DESC dummy_root_signature_desc = {};
        dummy_root_signature_desc.NumParameters             = 0;
        dummy_root_signature_desc.pParameters               = nullptr;
        DXCK(D3D12SerializeRootSignature(&dummy_root_signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, &dummy_root_signature, nullptr));
        DXCK(m_dx_device->CreateRootSignature(0,
                                              dummy_root_signature->GetBufferPointer(),
                                              dummy_root_signature->GetBufferSize(),
                                              IID_PPV_ARGS(&m_dx_empty_root_signature)));

        // create empty global root signature
        ComPtr<ID3DBlob>          local_dummy_root_signature      = nullptr;
        D3D12_ROOT_SIGNATURE_DESC local_dummy_root_signature_desc = {};
        local_dummy_root_signature_desc.NumParameters             = 0;
        local_dummy_root_signature_desc.pParameters               = nullptr;
        local_dummy_root_signature_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
        DXCK(D3D12SerializeRootSignature(&local_dummy_root_signature_desc,
                                         D3D_ROOT_SIGNATURE_VERSION_1,
                                         &local_dummy_root_signature,
                                         nullptr));
        DXCK(m_dx_device->CreateRootSignature(0,
                                              local_dummy_root_signature->GetBufferPointer(),
                                              local_dummy_root_signature->GetBufferSize(),
                                              IID_PPV_ARGS(&m_dx_empty_local_root_signature)));

    #ifdef USE_NSIGHT_AFTERMATH
        m_nv_gpu_crash_tracker.Initialize();
        const uint32_t aftermathFlags =
            GFSDK_Aftermath_FeatureFlags_EnableMarkers |          // Enable event marker tracking.
            GFSDK_Aftermath_FeatureFlags_EnableResourceTracking | // Enable tracking of resources.
            GFSDK_Aftermath_FeatureFlags_CallStackCapturing | // Capture call stacks for all draw calls, compute dispatches, and resource copies.
            GFSDK_Aftermath_FeatureFlags_GenerateShaderDebugInfo; // Generate debug information for shaders.
        AFTERMATH_CHECK_ERROR(
            GFSDK_Aftermath_DX12_Initialize(GFSDK_Aftermath_Version_API, aftermathFlags, m_dx_device.Get()));
    #endif
    }

    // Retrieve ns/ticks (tick = smallest period between Gpu's timestamps)
    float
    get_timestamp_period() const
    {
        UINT64 frequency;
        m_dx_direct_queue->GetTimestampFrequency(&frequency);
        return 1e9f / static_cast<float>(frequency);
    }

    std::pair<uint64_t, uint64_t>
    get_sync_calibrate_cpu_gpu_time(QueueType queue_type)
    {
        ComPtr<ID3D12CommandQueue> queue = nullptr;
        switch (queue_type)
        {
        case QueueType::Graphics:
            queue = m_dx_direct_queue;
            break;
        case QueueType::Compute:
            queue = m_dx_compute_queue;
            break;
        case QueueType::Transfer:
            queue = m_dx_copy_queue;
            break;
        default:
            Logger::Critical<true>(__FUNCTION__, " reach end switch case for queue_type");
            break;
        }

        uint64_t cpu_time;
        uint64_t gpu_time;
        queue->GetClockCalibration(&gpu_time, &cpu_time);
        return std::make_pair(cpu_time, gpu_time);
    }

    inline bool
    enable_debug() const
    {
    #ifdef NDEBUG
        return false;
    #else
        return m_debug;
    #endif
    }

    template <typename DxType>
    void
    name_dx_object(DxType & dx_type, const std::string & name = "") const
    {
        if (!name.empty())
        {
            const std::wstring wname(name.begin(), name.end());
            dx_type->SetName(wname.c_str());
        }
    }

private:
    bool m_debug = false;

    ComPtr<ID3D12Device5>
    create_device(IDXGIAdapter4 * dx_adapter, [[maybe_unused]] const bool debug)
    {
    #if 0
        if (debug)
        {
            // needed to prevent gpu crash from SetStablePowerState
            D3D12EnableExperimentalFeatures(0, nullptr, nullptr, nullptr);
        }
    #endif

        ComPtr<ID3D12Device5> dx_device;
        DXCK(D3D12CreateDevice(dx_adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&dx_device)));

    #if 0
        // note: stable power crashes renderdoc so I turned it off by default
        if (debug)
        {
            // set stable power state for easier time queries
            DXCK(dx_device2->SetStablePowerState(TRUE));

            // show warnings and errors in output
            ComPtr<ID3D12InfoQueue> info_queue;
            if (SUCCEEDED(dx_device2.As(&info_queue)))
            {
                // enable break on severity
                Logger::Info(__FUNCTION__ " device will breaks on severe DX12 messages");
                info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
                info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
                info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);
            }
        }
    #endif
        return dx_device;
    }

    ComPtr<ID3D12CommandQueue>
    create_command_queue(D3D12_COMMAND_LIST_TYPE type)
    {
        ComPtr<ID3D12CommandQueue> command_queue;
        D3D12_COMMAND_QUEUE_DESC   desc = {};
        {
            desc.Type     = type;
            desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
            desc.Flags    = D3D12_COMMAND_QUEUE_FLAG_NONE;
            desc.NodeMask = 0;
        }

        DXCK(m_dx_device->CreateCommandQueue(&desc, IID_PPV_ARGS(&command_queue)));
        return command_queue;
    }

    bool
    check_tearing_support()
    {
        return false;
        BOOL                  allow_tearing = FALSE;
        ComPtr<IDXGIFactory4> factory4;
        if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory4))))
        {
            ComPtr<IDXGIFactory5> factory5;
            if (SUCCEEDED(factory4.As(&factory5)))
            {
                if (FAILED(factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                                                         &allow_tearing,
                                                         sizeof(allow_tearing))))
                {
                    allow_tearing = FALSE;
                }
            }
        }
        return allow_tearing == TRUE;
    }

    bool
    check_raytracing_support()
    {
        D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
        DXCK(m_dx_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5)));
        return options5.RaytracingTier >= D3D12_RAYTRACING_TIER_1_0;
    }
};
} // namespace DXA_NAME
#endif