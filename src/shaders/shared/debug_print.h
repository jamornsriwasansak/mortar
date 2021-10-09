#ifndef DEBUG_PRINT_H
#define DEBUG_PRINT_H

struct DebugWriter
{
    uint32_t m_pointer;

    void
    init()
    {
        m_pointer = 0;
    }

    void
    write_char(const uint32_t character)
    {
        if (m_pointer % 4 == 0)
        {
            u_debug_char4[m_pointer / 4] = 0;
        }
        // note if this depends on endianness of each machine
#if 1
        const uint shift_power = m_pointer % 4;
#else
        const uint shift_power = 4 - m_pointer % 4;
#endif
        u_debug_char4[m_pointer / 4] += character << (shift_power * 8);
        m_pointer++;
    }

    void
    c(const uint32_t character)
    {
        write_char(character);
    }

    void
    write_uint(const uint32_t val)
    {
        if (val == 0)
        {
            write_char('0');
            return;
        }

        uint32_t v = val;
        uint32_t p = 1;
        while (v != 0)
        {
            v /= 10;
            p *= 10;
        }

        v = val;
        p /= 10;
        while (p != 0)
        {
            uint32_t q = v / p;
            write_char(q + '0');
            v -= q * p;
            p /= 10;
        }
    }
};

#endif