//#include "shaders/cpp_compatible.h"

//#define DEBUG_RayTraceVisualizePrintClickedInfo
//#define DEBUG_RayTraceVisualizePrintChar4BufferSize 1000

#ifndef RT_VISUALIZE_CB2_PARAMS
    #define RT_VISUALIZE_CB2_PARAMS

struct RaytracePathTracingCbParams
{
    float4x4 m_camera_inv_view;
    float4x4 m_camera_inv_proj;
};

    #ifdef __cplusplus
static_assert(sizeof(RaytracePathTracingCbParams) % (sizeof(uint32_t) * 4) == 0);
    #endif

    #ifdef DEBUG_RayTraceVisualizePrintClickedInfo
struct RaytraceVisualizeDebugPrintCbParams
{
    uint3 m_selected_thread_id;
    uint  m_flag;
};
    #endif

#endif