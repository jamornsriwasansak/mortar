#pragma once

#include "pch/pch.h"

namespace Std
{
// fixed-sized vector
template <typename Type, const size_t Length>
struct FsVector
{
    FsVector() {}

    void
    push_back(const Type & v)
    {
        m_array[m_size++] = v;
    }

    void
    emplace_back(Type && v)
    {
        m_array[m_size++] = std::move(v);
    }

    Type &
    operator[](const size_t index)
    {
        return m_array[index];
    }

    const Type &
    operator[](const size_t index) const
    {
        return m_array[index];
    }

    const Type *
    data() const
    {
        return m_array.data();
    }

    size_t
    length() const
    {
        return m_size;
    }

    size_t
    max_size() const
    {
        return Length;
    }

    void
    resize(const size_t size)
    {
        assert(size < Length);
        m_size = size;
    }

private:
    size_t m_size = 0;
    // std::array already implement assertion
    // assert(m_size < Length);
    std::array<Type, Length> m_array = {};
};
} // namespace Std
