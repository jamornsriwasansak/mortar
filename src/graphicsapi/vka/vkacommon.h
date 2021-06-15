#pragma once

#include "common/glfwhandler.h"

#include "../enums.h"
#include "../shadersrc.h"
#include "common/logger.h"
#include "common/uniquehandle.h"
//
#include <iostream>
#include <map>
#include <set>
#include <span>
#include <string>
#include <variant>
#include <vector>

#define VKCK(ResultEvalExpression)                                                                     \
    do                                                                                                 \
    {                                                                                                  \
        vk::Result result571481963 = vk::Result(ResultEvalExpression);                                 \
        if (result571481963 != vk::Result::eSuccess)                                                   \
        {                                                                                              \
            Logger::Error<true>(__FUNCTION__, " : ", __LINE__, " : ", vk::to_string(result571481963)); \
        }                                                                                              \
    } while (false)


namespace Vka
{
using ShaderSrc = TShaderSrc<Vka::ShaderStageEnum>;
} // namespace Vka
