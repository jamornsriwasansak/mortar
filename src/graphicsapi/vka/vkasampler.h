#pragma once
#include "vkacommon.h"
#include "vkadevice.h"

namespace Vka
{
struct Sampler
{
    vk::UniqueSampler m_vk_sampler;

    Sampler(const Device * device)
    {
        vk::SamplerCreateInfo sampler_ci;
        sampler_ci.setMagFilter(vk::Filter::eLinear);
        sampler_ci.setMinFilter(vk::Filter::eLinear);
        sampler_ci.setAddressModeU(vk::SamplerAddressMode::eRepeat);
        sampler_ci.setAddressModeV(vk::SamplerAddressMode::eRepeat);
        sampler_ci.setAddressModeW(vk::SamplerAddressMode::eRepeat);
        sampler_ci.setAnisotropyEnable(true);
        sampler_ci.setMaxAnisotropy(16);
        sampler_ci.setBorderColor(vk::BorderColor::eIntOpaqueBlack);
        sampler_ci.setUnnormalizedCoordinates(false);
        sampler_ci.setCompareEnable(false);
        sampler_ci.setCompareOp(vk::CompareOp::eAlways);
        sampler_ci.setMipmapMode(vk::SamplerMipmapMode::eLinear);
        sampler_ci.setMipLodBias(0.0f);
        sampler_ci.setMinLod(0.0f);
        sampler_ci.setMaxLod(0.0f);
        m_vk_sampler = device->m_vk_ldevice->createSamplerUnique(sampler_ci);
    }
};
} // namespace Vka