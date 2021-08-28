#ifndef RT_VISUALIZE_CB_PARAMS
#define RT_VISUALIZE_CB_PARAMS
struct RtVisualizeCbParams
{
    float4x4 m_camera_inv_view;
    float4x4 m_camera_inv_proj;
};
#endif