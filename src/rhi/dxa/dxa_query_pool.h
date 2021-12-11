#pragma once

#include "pch/pch.h"

#ifdef USE_DXA

    #include "dxa_device.h"

namespace DXA_NAME
{
struct QueryPool
{
    ComPtr<ID3D12QueryHeap> m_dx_query_heap;

    QueryPool(const std::string & name, const Device & device, const QueryType query_type, const uint32_t num_queries)
    : m_dx_query_heap(create_query_heap(device, query_type, num_queries))
    {
        device.name_dx_object(m_dx_query_heap, name);
    }

    static ComPtr<ID3D12QueryHeap>
    create_query_heap(const Device & device, const QueryType query_type, const uint32_t num_queries)
    {
        D3D12_QUERY_HEAP_DESC query_heap_desc = {};
        query_heap_desc.Count                 = num_queries;
        query_heap_desc.Type                  = static_cast<D3D12_QUERY_HEAP_TYPE>(query_type);

        ComPtr<ID3D12QueryHeap> result;
        DXCK(device.m_dx_device->CreateQueryHeap(&query_heap_desc, IID_PPV_ARGS(&result)));
        return result;
    }
};
} // namespace DXA_NAME
#endif