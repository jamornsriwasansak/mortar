#pragma once

#include "dxa_common.h"
#ifdef USE_DXA

    #include "dxa_device.h"

namespace DXA_NAME
{
struct DescriptorPool
{
    DescriptorHeap<DescriptorGpuCpuHandle> m_cbv_srv_uav_heap;
    DescriptorHeap<DescriptorGpuCpuHandle> m_sampler_heap;

    DescriptorPool(const std::string & name, const Device & device, const uint32_t num_descriptors)
    : m_cbv_srv_uav_heap(device.m_dx_device.Get(),
                         D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
                         D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                         num_descriptors),
      m_sampler_heap(device.m_dx_device.Get(), D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, num_descriptors)
    {
        device.name_dx_object(m_cbv_srv_uav_heap.m_dx_descriptor_heap, name + "_cbv_srv_uav_heap");
        device.name_dx_object(m_sampler_heap.m_dx_descriptor_heap, name + "_sampler_heap");
    }

    void
    reset()
    {
        m_cbv_srv_uav_heap.reset();
        m_sampler_heap.reset();
    }
};

} // namespace DXA_NAME
#endif