#pragma once

#include "pch/pch.h"

#ifdef USE_DXA

    #include "dxa_buffer.h"
    #include "dxa_device.h"

namespace DXA_NAME
{
struct QueryPool
{
    ComPtr<ID3D12QueryHeap> m_dx_query_heap;
    Buffer                  m_query_result_readback_buffer;

    QueryPool(const std::string & name, const Device & device, const QueryType query_type, const uint32_t num_queries)
    : m_dx_query_heap(construct_query_heap(device, query_type, num_queries)),
      m_query_result_readback_buffer(name + "_readback_buffer",
                                     device,
                                     static_cast<BufferUsageEnum>(D3D12_RESOURCE_STATE_COPY_DEST),
                                     MemoryUsageEnum::GpuToCpu,
                                     num_queries * sizeof(uint64_t))
    {
        device.name_dx_object(m_dx_query_heap, name);
    }

    static ComPtr<ID3D12QueryHeap>
    construct_query_heap(const Device & device, const QueryType query_type, const uint32_t num_queries)
    {
        D3D12_QUERY_HEAP_DESC query_heap_desc = {};
        query_heap_desc.Count                 = num_queries;
        query_heap_desc.Type                  = static_cast<D3D12_QUERY_HEAP_TYPE>(query_type);

        ComPtr<ID3D12QueryHeap> result;
        DXCK(device.m_dx_device->CreateQueryHeap(&query_heap_desc, IID_PPV_ARGS(&result)));
        return result;
    }

    void
    reset()
    {
        // do nothing
    }

    std::vector<uint64_t>
    get_query_result(const uint32_t num_queries)
    {
        std::vector<uint64_t> result(num_queries);
        const uint64_t * query_result = static_cast<uint64_t *>(m_query_result_readback_buffer.map());
        for (uint32_t i_query = 0; i_query < num_queries; i_query++)
        {
            result[i_query] = query_result[i_query];
        }
        m_query_result_readback_buffer.unmap();
        return result;
    }
};
} // namespace DXA_NAME
#endif