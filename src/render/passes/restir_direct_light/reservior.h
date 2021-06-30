#ifndef WEIGHTED_RESERVIOR_H
#define WEIGHTED_RESERVIOR_H

struct Reservior
{
    uint32_t m_geometry_id;
    uint32_t m_primitive_id;
    float2   m_uv;
    float    m_w_sum;
    float    m_padding[3];

    void
    init()
    {
        m_w_sum        = 0.0f;
        m_geometry_id  = 0;
        m_primitive_id = 0;
        m_uv           = half2(0, 0);
        m_padding[0]   = 0.0f;
        m_padding[1]   = 0.0f;
        m_padding[2]   = 0.0f;
    }

    bool
    update(const uint geometry_id, const uint primitive_id, const half2 uv, float w, float rnd)
    {
        m_w_sum = m_w_sum + w;
        if (rnd < (w / m_w_sum) || m_w_sum == 0.0f)
        {
            m_geometry_id  = geometry_id;
            m_primitive_id = primitive_id;
            m_uv           = uv;
            return true;
        }
        else
        {
            return false;
        }
    }
};

// TODO:: investigate why we cannot align by 16
#ifdef __cplusplus
static_assert(sizeof(Reservior) == 32, "size of reservior must be aligned by 32");
#endif

#endif