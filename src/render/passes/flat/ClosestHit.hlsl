#include "Common.hlsl"

[shader("closesthit")]
void ClosestHit(inout HitInfo payload, const Attributes attrib)
{
    float3 barycentrics = float3((1.0f - attrib.uv.x - attrib.uv.y), attrib.uv.x, attrib.uv.y);

    VertexAttributes vattrib = GetVertexAttributes(GeometryIndex(), PrimitiveIndex(), barycentrics);

    // fetch pbr material info
    uint material_id = material_ids[GeometryIndex()];
    PbrMaterial material = materials[material_id];

    //payload.ShadedColorAndHitT = float4(get_color(PrimitiveIndex()), 0.0f);

    //float colored_index = GeometryIndex() / 256.0f;
    //payload.ShadedColorAndHitT = float4(colored_index, colored_index, colored_index, 1.0f);
    float4 result = g_textures[material.m_roughness_tex_id].SampleLevel(g_sampler, vattrib.uv, 0).rgba;
    payload.ShadedColorAndHitT = g_textures[material.m_roughness_tex_id].SampleLevel(g_sampler, vattrib.uv, 0).rgba;

    //payload.ShadedColorAndHitT = float4(vattrib.position, 0.0f) + result * 0.00001f;
    //payload.ShadedColorAndHitT = float4(barycentrics + vattrib.position, 1.0f);

    //float3 indices = GetInterpolatedIndices(GeometryIndex(), PrimitiveIndex(), barycentrics) / 1024.0f;
    //payload.ShadedColorAndHitT = float4(indices, RayTCurrent()) + result * 0.00001f;
    //payload.ShadedColorAndHitT = float4(get_color(GeometryIndex()), 0.0f) + result * 0.00001f;
    //payload.ShadedColorAndHitT = float4(barycentric, 1.0f);
    //payload.ShadedColorAndHitT = float4(1.0f, 0.0f, 0.0f, 1.0f);
}