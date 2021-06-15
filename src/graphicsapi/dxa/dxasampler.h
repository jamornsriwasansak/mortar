#pragma once

#include "dxacommon.h"
#include "dxadevice.h"

namespace Dxa
{
struct Sampler
{
    D3D12_SAMPLER_DESC sampler_desc = {};
    Sampler(const Device * device)
    {
        sampler_desc.Filter         = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        sampler_desc.AddressU       = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler_desc.AddressV       = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler_desc.AddressW       = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler_desc.MinLOD         = 0;
        sampler_desc.MaxLOD         = D3D12_FLOAT32_MAX;
        sampler_desc.MipLODBias     = 0.0f;
        sampler_desc.MaxAnisotropy  = 1;
        sampler_desc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    }
};
} // namespace Dxa