#ifndef RT_VISUALIZE_CB_PARAMS
#define RT_VISUALIZE_CB_PARAMS

struct RtVisualizeCbParams
{
    static const int ModeInstanceId          = 0;
    static const int ModeGeometryId          = 1;
    static const int ModeTriangleId          = 2;
    static const int ModeBaryCentricCoords   = 3;
    static const int ModePosition            = 4;
    static const int ModeNormal              = 5;
    static const int ModeDepth               = 6;
    static const int ModeDiffuseReflectance  = 7;
    static const int ModeSpecularReflectance = 8;

    float4x4 m_camera_inv_view;
    float4x4 m_camera_inv_proj;
    int      m_mode;
};
#endif