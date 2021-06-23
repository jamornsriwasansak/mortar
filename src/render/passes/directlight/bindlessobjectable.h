#ifndef BINDLESS_OBJECT_TABLE_PARAMS_H
#define BINDLESS_OBJECT_TABLE_PARAMS_H

struct BindlessObjectTable
{
    int m_nonemissive_object_start_index;
    int m_num_nonemissive_objects;
    int m_emissive_object_start_index;
    int m_num_emissive_objects;
};
#endif