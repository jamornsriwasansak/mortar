#ifndef RT_VISUALIZE_CB_PARAMS
#define RT_VISUALIZE_CB_PARAMS

struct RaytraceVisualizeCbParams
{
    static const int ModeInstanceId          = 0;
    static const int ModeBaseInstanceId      = 1;
    static const int ModeGeometryId          = 2;
    static const int ModeTriangleId          = 3;
    static const int ModeBaryCentricCoords   = 4;
    static const int ModePosition            = 5;
    static const int ModeGeometryNormal      = 6;
    static const int ModeTextureCoords       = 7;
    static const int ModeDepth               = 8;
    static const int ModeDiffuseReflectance  = 9;
    static const int ModeSpecularReflectance = 10;
    static const int ModeIndex               = 11;

    float4x4 m_camera_inv_view;
    float4x4 m_camera_inv_proj;
    int m_mode;
};
#endif