#pragma once

#include "dxa_common.h"
#ifdef USE_DXA

#include "core/uniquehandle.h"

namespace DXA_NAME
{
struct Fence
{
    struct FenceHandleDeleter
    {
        void
        operator()(HANDLE handle)
        {
            assert(handle != nullptr);
            ::CloseHandle(handle);
        }
    };

    ComPtr<ID3D12Fence> m_dx_fence = nullptr;
    UINT64 m_expected_fence_value = 0;
    UniquePtrHandle<HANDLE, FenceHandleDeleter> m_fence_handle;

    Fence() {}

    Fence(const std::string & name, const Device & device)
    {
        m_expected_fence_value = 1;

        // create fence
        DXCK(device.m_dx_device->CreateFence(m_expected_fence_value, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_dx_fence)));

        // create fence handle
        *m_fence_handle = ::CreateEvent(NULL, FALSE, FALSE, NULL);
        if (!m_fence_handle.get())
        {
            Logger::Critical<true>("cannot create fence handle");
        }

        // name fence
        device.name_dx_object(m_dx_fence, name);
    }

    void
    wait(std::chrono::milliseconds duration = std::chrono::milliseconds::max())
    {
        if (m_dx_fence->GetCompletedValue() < m_expected_fence_value)
        {
            DXCK(m_dx_fence->SetEventOnCompletion(m_expected_fence_value, *m_fence_handle));
            ::WaitForSingleObjectEx(*m_fence_handle, static_cast<DWORD>(duration.count()), FALSE);
        }
    }

    void
    reset()
    {
        m_expected_fence_value++;
    }
};
} // namespace DXA_NAME
#endif
