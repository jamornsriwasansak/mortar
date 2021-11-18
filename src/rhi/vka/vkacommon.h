#pragma once

#include "rhi/setting.h"

#ifdef USE_VKA

#include "rhi/commontypes/rhienums.h"
#include "rhi/commontypes/rhishadersrc.h"
//
#include "core/glfwhandler.h"
#include "core/logger.h"
#include "core/uniquehandle.h"
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


namespace VKA_NAME
{
using ShaderSrc = Rhi::TShaderSrc<VKA_NAME::ShaderStageEnum>;
} // namespace VKA_NAME
#endif
