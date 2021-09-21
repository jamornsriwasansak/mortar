#pragma once

#pragma warning(push, 0)
#include <glm/glm.hpp>
#include <glm/gtx/compatibility.hpp>
#include <glm/gtx/string_cast.hpp>
#pragma warning(pop)
#include <cstdint>

using namespace glm;

using uint2 = glm::uvec2;
using uint3 = glm::uvec3;
using uint4 = glm::uvec4;
using half  = glm::u16;
using half2 = glm::u16vec2;
using half3 = glm::u16vec3;
using half4 = glm::u16vec4;

struct urange
{
    uint m_begin;
    uint m_end;

    urange() {}

    urange(const uint begin, const uint end) : m_begin(begin), m_end(end) {}
};

template <typename T>
constexpr const T
digit(const T num, const uint32_t i_digit)
{
    T p0 = 1;
    for (size_t i = 0; i < i_digit; i++) p0 *= T(10);
    const T value = num / p0;
    return value % 10;
}

template <typename T, typename U>
T
round_up(const T size, const U min_align_size)
{
    // min align size = 0, any size is ok
    if (min_align_size == 0)
    {
        return size;
    }
    return (size + min_align_size - 1) & ~(min_align_size - 1);
}

inline size_t
div_ceil(const size_t numerator, const size_t denominator)
{
    return (numerator + denominator - 1) / denominator;
}
