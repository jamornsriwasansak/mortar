#pragma once

#include "noncopyable.h"

struct UniquePtrHandleDefaultDeleter
{
    UniquePtrHandleDefaultDeleter() {}

    void
    operator()()
    {
    }
};


// a unique handle for local variable
// this is to avoid tedious rule of five
template <typename Type, typename Deleter = UniquePtrHandleDefaultDeleter>
struct UniqueVarHandle
{
    MAKE_NONCOPYABLE(UniqueVarHandle);

    UniqueVarHandle() {}

    ~UniqueVarHandle()
    {
        if (m_value.has_value())
        {
            Deleter()(m_value.value());
        }
    }

    UniqueVarHandle(UniqueVarHandle && rhs)
    {
        if (rhs.m_value.has_value())
        {
            m_value = rhs.m_value.value();
            rhs.m_value.reset();
        }
    }

    UniqueVarHandle &
    operator=(UniqueVarHandle && rhs)
    {
        if (this != &rhs)
        {
            if (rhs.m_value.has_value())
            {
                m_value = rhs.m_value.value();
                rhs.m_value.reset();
            }
        }
        return *this;
    }

    UniqueVarHandle(Type & rhs) { m_value = rhs; }

    UniqueVarHandle(const Type & rhs) { m_value = rhs; }

    Type const *
    operator->() const
    {
        assert(m_value.has_value());
        return &m_value.value();
    }

    Type *
    operator->()
    {
        assert(m_value.has_value());
        return &m_value.value();
    }

    bool
    empty() const
    {
        return !(m_value.has_value());
    }

    const Type &
    get() const
    {
        return m_value.value();
    }

    Type &
    get()
    {
        return m_value.value();
    }

    std::optional<Type> m_value = std::nullopt;
};

// a unique handle for local variable
// this is to avoid tedious rule of five
template <typename Type, typename Deleter = UniquePtrHandleDefaultDeleter>
struct UniquePtrHandle
{
    MAKE_NONCOPYABLE(UniquePtrHandle);

    UniquePtrHandle() {}

    ~UniquePtrHandle()
    {
        if (m_value)
        {
            Deleter()(m_value);
        }
    }

    UniquePtrHandle(UniquePtrHandle && rhs)
    {
        m_value     = rhs.m_value;
        rhs.m_value = nullptr;
    }

    UniquePtrHandle &
    operator=(UniquePtrHandle && rhs)
    {
        if (this != &rhs)
        {
            m_value     = rhs.m_value;
            rhs.m_value = nullptr;
        }
        return *this;
    }

    Type const *
    operator->() const
    {
        return &m_value;
    }

    Type *
    operator->()
    {
        return &m_value;
    }

    Type const &
    operator*() const
    {
        return m_value;
    }

    Type &
    operator*()
    {
        return m_value;
    }

    const Type &
    get() const
    {
        return m_value;
    }

    Type &
    get()
    {
        return m_value;
    }

    Type m_value = nullptr;
};
