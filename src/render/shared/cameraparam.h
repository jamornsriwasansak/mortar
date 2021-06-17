#ifndef CAMERA_PARAMS
#define CAMERA_PARAMS
#include "shared.h"
struct CameraParams
{
    float4x4 m_inv_view;
    float4x4 m_inv_proj;
};
#endif
