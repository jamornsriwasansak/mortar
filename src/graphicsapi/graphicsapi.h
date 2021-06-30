#pragma once

#if 1
#include "graphicsapi/vka/vka.h"
namespace Gp = Vka;
#else
#include "graphicsapi/dxa/dxa.h"
namespace Gp = Dxa;
#endif
