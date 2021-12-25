#pragma once

#ifdef __hlsl
    #define RAY_GEN_SHADER [shader("raygeneration")]
    #define CLOSEST_HIT_SHADER [shader("closesthit")]
    #define MISS_SHADER [shader("miss")]

    #define REGISTER(SPACE, VARIABLE_NAME, TYPE, BINDING) \
    VARIABLE_NAME:                                        \
        register(TYPE##BINDING, space##SPACE)

    #define REGISTER_ARRAY(SPACE, VARIABLE_NAME, ARRAY_SIZE, TYPE, BINDING) \
        VARIABLE_NAME[ARRAY_SIZE] : register(TYPE##BINDING, space##SPACE)

    #define REGISTER_WRAP_BEGIN(NAME)
    #define REGISTER_WRAP_END
#else

    #include "pch/pch.h"
    #include "rhi/rhi.h"

struct DescriptorsBase
{
    std::span<Rhi::DescriptorSet> m_descriptor_sets;
    DescriptorsBase(std::span<Rhi::DescriptorSet> descriptor_sets)
    : m_descriptor_sets(descriptor_sets)
    {
    }
};

    #define REGISTER_WRAP_BEGIN(NAME)                                                              \
        struct NAME : public DescriptorsBase                                                       \
        {                                                                                          \
            NAME(std::span<Rhi::DescriptorSet> descriptor_sets) : DescriptorsBase(descriptor_sets) \
            {                                                                                      \
            }

    #define REGISTER_WRAP_END \
        }                     \
        ;

struct RegisterBase
{
    uint16_t          m_binding;
    uint16_t          m_space;
    size_t            m_array_size;
    DescriptorsBase * m_descriptors_base;
};

struct SamplerState : public RegisterBase
{
    void
    set(const Rhi::Sampler & sampler)
    {
        m_descriptors_base->m_descriptor_sets[m_space].set_s_sampler(m_binding, sampler);
    }
};

struct RaytracingAccelerationStructure : public RegisterBase
{
    void
    set(const Rhi::RayTracingTlas & buffer)
    {
        m_descriptors_base->m_descriptor_sets[m_space].set_t_ray_tracing_accel(m_binding, buffer);
    }
};

template <typename T>
struct RWTexture2D : public RegisterBase
{
    static constexpr size_t ElementSize = sizeof(T);
    void
    set(const Rhi::Texture & texture)
    {
        m_descriptors_base->m_descriptor_sets[m_space].set_u_rw_texture(m_binding, texture);
    }
};

template <typename T>
struct Texture2D : public RegisterBase
{
    static constexpr size_t ElementSize = sizeof(T);
    void
    set(const Rhi::Texture & texture, const size_t dst_element_index)
    {
        m_descriptors_base->m_descriptor_sets[m_space].set_t_texture(m_binding, texture, dst_element_index);
    }
};

template <typename T>
struct ConstantBuffer : public RegisterBase
{
    static constexpr size_t ElementSize = sizeof(T);
    void
    set(const Rhi::Buffer & buffer)
    {
        m_descriptors_base->m_descriptor_sets[m_space].set_b_constant_buffer(m_binding, buffer);
    }
};


template <typename T>
struct StructuredBuffer : public RegisterBase
{
    static constexpr size_t ElementSize = sizeof(T);
    void
    set(const Rhi::Buffer & buffer,
        const size_t        dst_element_index = 0,
        const size_t        stride            = 0,
        const size_t        num_elements      = 0,
        const size_t        src_element_index = 0)
    {
        size_t true_stride_size = stride == 0 ? ElementSize : stride;
        m_descriptors_base->m_descriptor_sets[m_space].set_t_structured_buffer(m_binding,
                                                                               buffer,
                                                                               true_stride_size,
                                                                               num_elements,
                                                                               dst_element_index,
                                                                               src_element_index);
    }
};

template <typename T>
struct RWStructuredBuffer : public RegisterBase
{
    static constexpr size_t ElementSize = sizeof(T);
    void
    set(const Rhi::Buffer & buffer,
        const size_t        dst_element_index = 0,
        const size_t        stride            = 0,
        const size_t        num_elements      = 0,
        const size_t        src_element_index = 0)
    {
        size_t true_stride_size = stride == 0 ? ElementSize : stride;
        m_descriptors_base->m_descriptor_sets[m_space].set_u_rw_structured_buffer(m_binding,
                                                                                  buffer,
                                                                                  true_stride_size,
                                                                                  num_elements,
                                                                                  dst_element_index,
                                                                                  src_element_index);
    }
};

    #define REGISTER_ARRAY(SPACE, VARIABLE_NAME, ARRAY_SIZE, TYPE, BINDING) \
        VARIABLE_NAME = { BINDING, SPACE, ARRAY_SIZE, this };

    #define REGISTER(SPACE, VARIABLE_NAME, TYPE, BINDING) \
        VARIABLE_NAME = { BINDING, SPACE, 1, this };
#endif