#ifndef PBR_OBJECT_H
#define PBR_OBJECT_H

struct PbrMesh
{
    int m_vertex_buffer_id     = -1;
    int m_index_buffer_id      = -1;
    int m_vertex_buffer_offset = -1;
    int m_index_buffer_offset  = -1;
    int m_num_vertices         = -1;
    int m_num_indices          = -1;
    int m_material_id          = -1;
    int m_emissive_id          = -1;
};

#endif