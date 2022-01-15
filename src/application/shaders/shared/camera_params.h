#ifndef CAMERA_PARAMS
#define CAMERA_PARAMS
struct CameraParams
{
    float4x4 m_inv_view;
    float4x4 m_inv_proj;
};
#endif
