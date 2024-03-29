#ifndef BINDLESS_TABLE_H
#define BINDLESS_TABLE_H

#include "types.h"

// In bindless rendering, InstanceId, GeometryId and PrimitiveId are provided.
//
// To reference a geometry information, we need a geometry table.
// The correct geometry index can be referenced as follows:
// GeometryTableEntries[BaseInstanceTableEntries[InstanceId].m_geometry_table_index_base + GeometryId]
//
// There is a special case: DeviceBaseInstances[0] == 0, m_geometry_table_index_base of BaseInstanceTableEntry will always be 0
// We can use this knowledge to skip one dependency fetch

struct GeometryTableEntry
{
    uint32_t m_vertex_base_index;
    uint32_t m_index_base_index;
    uint32_t m_material_index;
    uint32_t m_emission_index;
};

struct BaseInstanceTableEntry
{
    uint16_t m_geometry_table_index_base;
};

#endif // BINDLESS_TABLE_H