#ifndef COMPACT_VERTEX_H
#define COMPACT_VERTEX_H

struct CompactVertex
{
    float3 m_snormal;
    float2 m_texcoord;

    void
    set_snormal(const float3 snormal)
    {
#ifdef __cplusplus
        assert(abs(1.0f - length(snormal)) <= 0.001f);
#endif
        m_snormal.x = snormal.x;
        m_snormal.y = snormal.y;
        m_snormal.z = snormal.z;
    }

    float3
    get_snormal()
    {
        //const float z = sqrt(1.0f - m_snormal.x * m_snormal.x - m_snormal.y * m_snormal.y);
        return float3(m_snormal.x, m_snormal.y, m_snormal.z);
    }
};

#endif // COMPACT_VERTEX_H
