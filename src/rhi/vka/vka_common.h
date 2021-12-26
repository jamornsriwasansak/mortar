#pragma once

#include "pch/pch.h"

#ifdef USE_VKA

    #include "core/glfwhandler.h"
    #include "core/logger.h"
    #include "core/uniquehandle.h"
    #include "rhi/common/rhi_enums.h"
    #include "rhi/common/rhi_shader_src.h"
    #include "rhi/common/rhi_texture_create_info.h"

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

using TextureCreateInfo =
    RhiCommon::TTextureCreateInfo<VKA_NAME::FormatEnum, VKA_NAME::TextureUsageEnum, vk::ImageCreateInfo>;

using DeviceSizeT = vk::DeviceSize;
} // namespace VKA_NAME
#endif
