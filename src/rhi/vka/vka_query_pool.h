#pragma once

#include "pch/pch.h"

#ifdef USE_VKA

    #include "vka_device.h"

namespace VKA_NAME
{
struct QueryPool
{
    vk::UniqueQueryPool m_vk_query_pool;
    const Device &      m_device;
    const uint32_t      m_num_queries;

    QueryPool(const std::string & name, const Device & device, const QueryType query_type, const uint32_t num_queries)
    : m_vk_query_pool(construct_query_pool(device, query_type, num_queries)),
      m_device(device),
      m_num_queries(num_queries)
    {
        m_device.name_vkhpp_object(m_vk_query_pool.get(), name);
    }

    static vk::UniqueQueryPool
    construct_query_pool(const Device & device, const QueryType query_type, const uint32_t num_queries)
    {
        vk::QueryPoolCreateInfo query_pool_ci = {};
        query_pool_ci.setQueryCount(num_queries);
        query_pool_ci.setQueryType(static_cast<vk::QueryType>(query_type));
        return device.m_vk_ldevice->createQueryPoolUnique(query_pool_ci);
    }
};
} // namespace VKA_NAME
#endif
