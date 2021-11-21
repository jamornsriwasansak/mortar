#pragma once

//#define USE_VKA
#define USE_DXA

#define VKA_NAME RhiVka
#define DXA_NAME RhiDxa

#if 0
#undef VKA_NAME
#define VKA_NAME Rhi
#define USE_VKA
#else
#undef DXA_NAME
#define DXA_NAME Rhi
#define USE_DXA
#endif