#ifndef RT_VISUALIZE_CB_PARAMS
#define RT_VISUALIZE_CB_PARAMS

enum class RaytraceVisualizeModeEnum : uint16_t
{
    ModeInstanceId,
    ModeBaseInstanceId,
    ModeGeometryId,
    ModeTriangleId,
    ModeBaryCentricCoords,
    ModePosition,
    ModeGeometryNormal,
    ModeShadingNormal,
    ModeTextureCoords,
    ModeDepth,
    ModeDiffuseReflectance,
    ModeSpecularReflectance,
    ModeRoughness
};

struct RaytraceVisualizeCbParams
{
    float4x4 m_camera_inv_view;
    float4x4 m_camera_inv_proj;
    RaytraceVisualizeModeEnum m_mode;
};
#endif