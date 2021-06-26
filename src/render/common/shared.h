#ifndef SHAREDCOMMON_H
#define SHAREDCOMMON_H

#ifdef __cplusplus
#include "common/vmath.h"
#define SEMANTIC(sname)
#else
#define SEMANTIC(sname) : sname
#endif

#endif //SHAREDCOMMON_H
