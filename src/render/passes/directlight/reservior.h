#ifndef WEIGHTED_RESERVIOR_H
#define WEIGHTED_RESERVIOR_H

struct Reservior
{
    float3 m_y;
    float  m_p_y;
    //
    float  m_selected_w;
    float  m_w_sum;
    float  m_num_samples;
    float  m_inv_pdf;
    //
    float2 m_y_pss;
    float  padding0;
    float  padding1;

    bool
    update(float3 y, float p_y, float w, float num_samples, float rnd)
    {
        m_w_sum = m_w_sum + w;
        m_num_samples += num_samples;
        if (rnd < (w / m_w_sum) || m_w_sum == 0.0f)
        {
            m_y          = y;
            m_p_y        = p_y;
            m_selected_w = w;
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
    reservior.m_y           = float3(0.0f, 0.0f, 0.0f);
    reservior.m_p_y         = 0.0f;
    reservior.m_selected_w  = 0.0f;
    reservior.m_w_sum       = 0.0f;
    reservior.m_num_samples = 0.0f;
    reservior.m_inv_pdf     = 0.0f;
    return reservior;
}

#endif