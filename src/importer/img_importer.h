#pragma once

#include "pch/pch.h"
#include "rhi/rhi.h"

struct ImgImporter
{
    Rhi::Texture
    add_texture_from_path(const std::filesystem::path & path,
                          const size_t                  desired_channel,
                          Rhi::StagingBufferManager *   staging_buffer_manager)
    {
        const std::string filepath_str = path.string();
        // load using stbi
        int2 resolution;
        stbi_set_flip_vertically_on_load(true);
        void * image =
            stbi_load(filepath_str.c_str(), &resolution.x, &resolution.y, nullptr, static_cast<int>(desired_channel));
        std::byte * image_bytes = reinterpret_cast<std::byte *>(image);
        assert(desired_channel == 4 || desired_channel == 1);
        Rhi::FormatEnum format_enum =
            desired_channel == 4 ? Rhi::FormatEnum::R8G8B8A8_UNorm : Rhi::FormatEnum::R8_UNorm;
        Rhi::Texture dst(&staging_buffer_manager->m_device,
                         Rhi::TextureUsageEnum::Sampled,
                         Rhi::TextureStateEnum::FragmentShaderVisible,
                         format_enum,
                         resolution,
                         image_bytes,
                         staging_buffer_manager,
                         float4(),
                         filepath_str);
        staging_buffer_manager->submit_all_pending_upload();
        stbi_image_free(image);
        return dst;
    }
};