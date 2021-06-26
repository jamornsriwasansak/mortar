#ifndef SHAREDCOMMON_H
#define SHAREDCOMMON_H

#ifdef __cplusplus
// float4, float3, ....
#include "common/vmath.h"
#else
#endif

#ifdef __cplusplus
#define SEMANTIC(sname)
#else
#define SEMANTIC(sname) : sname
#endif

#ifdef __cplusplus
#define INOUT(argument) argument &
#else
#define INOUT(argument) inout argument
#endif

#ifdef __cplusplus
#define REGISTER(r, space)
#else
#define REGISTER(r, space) : register(r, space);
#endif

#ifdef __cplusplus
#define SHADER_TYPE(name)
#else
#define SHADER_TYPE(name) [shader(name)]
#endif 

#endif //SHAREDCOMMON_H
