#pragma warning(push, 0)
#include <glm/glm.hpp>
#include <glm/gtc/packing.hpp>
#include <glm/gtx/compatibility.hpp>
#include <glm/gtx/string_cast.hpp>
#pragma warning(pop)
#include <cstdint>

using namespace glm;

using uint2 = glm::uvec2;
using uint3 = glm::uvec3;
using uint4 = glm::uvec4;

template <uint8_t NumElements, glm::qualifier Qualifier = glm::qualifier::defaultp>
struct halfN
{
    glm::vec<NumElements, uint16_t, Qualifier> val;

    halfN() {}

    halfN(const glm::vec<NumElements, float, Qualifier> & v)
    {
        static_assert(sizeof(uint16_t) == sizeof(short),
                      "To use glm::toFloat16, sizeof uint16_t must strictly equals to short");

        if constexpr (NumElements == 1)
        {
            val[0] = glm::detail::toFloat16(v[0]);
        }
        else if constexpr (NumElements == 2)
        {
            val[0] = glm::detail::toFloat16(v[0]);
            val[1] = glm::detail::toFloat16(v[1]);
        }
        else if constexpr (NumElements == 3)
        {
            val[0] = glm::detail::toFloat16(v[0]);
            val[1] = glm::detail::toFloat16(v[1]);
            val[2] = glm::detail::toFloat16(v[2]);
        }
        else if constexpr (NumElements == 4)
        {
            val[0] = glm::detail::toFloat16(v[0]);
            val[1] = glm::detail::toFloat16(v[1]);
            val[2] = glm::detail::toFloat16(v[2]);
            val[3] = glm::detail::toFloat16(v[3]);
        }
        else
        {
            static_assert(false, "halfN only support 4 >= NumElements >= 0");
        }
    }
};
using half  = halfN<1>;
using half2 = halfN<2>;
using half3 = halfN<3>;
using half4 = halfN<4>;

template <typename T>
struct rangeT
{
    T m_begin;
    T m_end;

    rangeT() {}

    rangeT(const T begin, const T end) : m_begin(begin), m_end(end) {}

    T
    length() const
    {
        return m_end - m_begin;
    }

    std::string
    to_string() const
    {
        return "range(" + std::to_string(m_begin) + ", " + std::to_string(m_end) + ")";
    }
};
using urange32_t = rangeT<uint32_t>;
using urange64_t = rangeT<uint64_t>;

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
    static_assert(!std::is_signed<T>::value);
    static_assert(!std::is_signed<U>::value);
    static_assert(std::numeric_limits<T>::max() >= std::numeric_limits<U>::max());
    assert(std::numeric_limits<T>::max() - min_align_size + 1 > size);

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
