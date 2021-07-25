#pragma once

#include "graphicsapi/graphicsapi.h"
#include <cstddef>
#include <stb_image.h>

struct ImgLoader
{
    void
    add_texture_from_path(Gp::Texture *                 dst,
                          const std::filesystem::path & path,
                          const size_t                  desired_channel,
                          Gp::StagingBufferManager *    staging_buffer_manager)
    {
        const std::string filepath_str = path.string();
        // load using stbi
        int2 resolution;
        stbi_set_flip_vertically_on_load(true);
        void * image = stbi_load(filepath_str.c_str(), &resolution.x, &resolution.y, nullptr, desired_channel);
        std::byte * image_bytes = reinterpret_cast<std::byte *>(image);
        assert(desired_channel == 4 || desired_channel == 1);
        Gp::FormatEnum format_enum =
            desired_channel == 4 ? Gp::FormatEnum::R8G8B8A8_UNorm : Gp::FormatEnum::R8_UNorm;
        *dst = Gp::Texture(staging_buffer_manager->m_device,
                           Gp::TextureUsageEnum::Sampled,
                           Gp::TextureStateEnum::FragmentShaderVisible,
                           format_enum,
                           resolution,
                           image_bytes,
                           staging_buffer_manager,
                           float4(),
                           filepath_str);
        staging_buffer_manager->submit_all_pending_upload();
        stbi_image_free(image);
    }
};