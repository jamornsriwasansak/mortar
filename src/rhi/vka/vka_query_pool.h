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
    bool                m_is_ready_for_query = false;

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

    std::vector<uint64_t>
    get_query_result(const uint32_t num_queries)
    {
        std::vector<uint64_t> result(num_queries);
        if (num_queries == 0) return result;
        m_device.m_vk_ldevice->getQueryPoolResults(m_vk_query_pool.get(),
                                                   0,
                                                   num_queries,
                                                   sizeof(result[0]) * result.size(),
                                                   result.data(),
                                                   0,
                                                   vk::QueryResultFlagBits::e64);
        return result;
    }
};
} // namespace VKA_NAME
#endif
