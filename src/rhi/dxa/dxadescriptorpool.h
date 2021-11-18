#pragma once

#include "dxacommon.h"
#ifdef USE_DXA

#include "dxadevice.h"

namespace DXA_NAME
{

// TODO:: needs a huge face life
struct DescriptorPool
{
    DescriptorHeap m_cbv_srv_uav_heap;
    DescriptorHeap m_sampler_heap;

    static constexpr size_t NumDescriptors = 500;

    DescriptorPool() {}

    DescriptorPool(Device * device)
    : m_cbv_srv_uav_heap(device->m_dx_device.Get(),
                         D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
                         D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                         NumDescriptors),
      m_sampler_heap(device->m_dx_device.Get(), D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, NumDescriptors)
    {
    }

    void
    reset()
    {
        m_cbv_srv_uav_heap.reset();
        m_sampler_heap.reset();
    }
};

} // namespace Dxa
#endif