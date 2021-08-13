#pragma once

#include "common/glfwhandler.h"

#include "../gpcommontypes/enums.h"
#include "../gpcommontypes/setting.h"
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


namespace VKA_NAME { using ShaderSrc = TShaderSrc<VKA_NAME::ShaderStageEnum>; } // namespace VKA_NAME
