#ifndef WEIGHTED_RESERVIOR_H
#define WEIGHTED_RESERVIOR_H

struct Reservior
{
    float3 m_y;
    float  m_p_y;
    //
    float m_selected_w;
    float m_w_sum;
    float m_num_samples;
    float m_inv_pdf;
    //
    float4 m_y_pss;

    void
    init()
    {
        m_y           = float3(0.0f, 0.0f, 0.0f);
        m_p_y         = 0.0f;
        m_selected_w  = 0.0f;
        m_w_sum       = 0.0f;
        m_num_samples = 0.0f;
        m_inv_pdf     = 0.0f;
        m_y_pss       = float4(0.0f, 0.0f, 0.0f, 0.0f);
    }

    bool
    update(float4 y_pss, float3 y, float p_y, float w, float num_samples, float rnd)
    {
        m_w_sum = m_w_sum + w;
        m_num_samples += num_samples;
        if (rnd < (w / m_w_sum) || m_w_sum == 0.0f)
        {
            m_y          = y;
            m_p_y        = p_y;
            m_selected_w = w;
            m_y_pss      = y_pss;
            return true;
        }
        else
        {
            return false;
        }
    }
};

#endif