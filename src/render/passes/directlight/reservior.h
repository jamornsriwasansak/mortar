#ifndef WEIGHTED_RESERVIOR_H
#define WEIGHTED_RESERVIOR_H

struct Reservior
{
    float3 m_y;
    float  m_p_y;
    float  m_m;
    float  m_w_sum;
    float  m_w;
    float  m_padding_1;

    bool
    update(float3 y, float w, float rnd)
    {
        m_w_sum = m_w_sum + w;
        m_m += 1.0f;
        if (rnd < (w / m_w_sum))
        {
            m_y   = y;
            m_p_y = w;
            return true;
        }
        else
        {
            return false;
        }
    }
};

Reservior
Reservior_create()
{
    Reservior reservior;
    reservior.m_w_sum = 0.0f;
    reservior.m_m     = 0.0f;
    return reservior;
}

#endif