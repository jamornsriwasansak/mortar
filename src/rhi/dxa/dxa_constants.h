#pragma once

#include "pch/pch.h"

#ifdef USE_DXA

    #include "rhi/common/rhi_enums.h"

namespace DXA_NAME
{
D3D12_RESOURCE_STATES
GetD3D12_RESOURCE_STATES(const Rhi::BufferUsageEnum buffer_usage)
{
    D3D12_RESOURCE_STATES result = D3D12_RESOURCE_STATE_COMMON;
    if (Rhi::HasFlag(buffer_usage, Rhi::BufferUsageEnum::VertexBuffer))
        result = result | D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    if (Rhi::HasFlag(buffer_usage, Rhi::BufferUsageEnum::IndexBuffer))
        result = result | D3D12_RESOURCE_STATE_INDEX_BUFFER;
    if (Rhi::HasFlag(buffer_usage, Rhi::BufferUsageEnum::RayTracingAccelStructBufferInput))
        result = result;
    if (Rhi::HasFlag(buffer_usage, Rhi::BufferUsageEnum::ConstantBuffer))
        result = result | D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    if (Rhi::HasFlag(buffer_usage, Rhi::BufferUsageEnum::StorageBuffer))
        result = result | D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    if (Rhi::HasFlag(buffer_usage, Rhi::BufferUsageEnum::TransferSrc))
        result = result | D3D12_RESOURCE_STATE_COPY_SOURCE;
    if (Rhi::HasFlag(buffer_usage, Rhi::BufferUsageEnum::TransferDst))
        result = result | D3D12_RESOURCE_STATE_COPY_DEST;
    if (Rhi::HasFlag(buffer_usage, Rhi::BufferUsageEnum::RayTracingAccelStructBuffer))
        result = result | D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
    return result;
}

D3D12_RESOURCE_STATES
GetD3D12_RESOURCE_STATES(const Rhi::TextureStateEnum texture_state)
{
    switch (texture_state)
    {
    case Rhi::TextureStateEnum::ColorAttachment:
        return D3D12_RESOURCE_STATE_RENDER_TARGET;
    case Rhi::TextureStateEnum::DepthAttachment:
        return D3D12_RESOURCE_STATE_DEPTH_WRITE;
    case Rhi::TextureStateEnum::ReadOnly:
        return D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
    case Rhi::TextureStateEnum::ReadWrite:
        return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    case Rhi::TextureStateEnum::Present:
        return D3D12_RESOURCE_STATE_PRESENT;
    case Rhi::TextureStateEnum::TransferSrc:
        return D3D12_RESOURCE_STATE_COPY_SOURCE;
    case Rhi::TextureStateEnum::TransferDst:
        return D3D12_RESOURCE_STATE_COPY_DEST;
    default:
        assert(false && "Program should not reach this state");
        return D3D12_RESOURCE_STATE_COMMON;
    }
}

DXGI_FORMAT
GetDXGI_FORMAT(const Rhi::FormatEnum format)
{
    switch (format)
    {
    case Rhi::FormatEnum::Depth32_SFloat:
        return DXGI_FORMAT_D32_FLOAT;
    case Rhi::FormatEnum::R10G10B10A2_UNorm:
        return DXGI_FORMAT_R10G10B10A2_UNORM;
    case Rhi::FormatEnum::R11G11B10_UFloat:
        return DXGI_FORMAT_R11G11B10_FLOAT;
    case Rhi::FormatEnum::R16G16B16A16_SNorm:
        return DXGI_FORMAT_R16G16B16A16_SNORM;
    case Rhi::FormatEnum::R16G16_SNorm:
        return DXGI_FORMAT_R16G16_SNORM;
    case Rhi::FormatEnum::R16_SFloat:
        return DXGI_FORMAT_R16_FLOAT;
    case Rhi::FormatEnum::R16_UInt:
        return DXGI_FORMAT_R16_UINT;
    case Rhi::FormatEnum::R16_UNorm:
        return DXGI_FORMAT_R16_UNORM;
    case Rhi::FormatEnum::R32G32B32A32_SFloat:
        return DXGI_FORMAT_R32G32B32A32_FLOAT;
    case Rhi::FormatEnum::R32G32B32_SFloat:
        return DXGI_FORMAT_R32G32B32_FLOAT;
    case Rhi::FormatEnum::R32_SFloat:
        return DXGI_FORMAT_R32_FLOAT;
    case Rhi::FormatEnum::R32_UInt:
        return DXGI_FORMAT_R32_UINT;
    case Rhi::FormatEnum::R8G8B8A8_UNorm_Srgb:
        return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    case Rhi::FormatEnum::R8G8B8A8_UNorm:
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    case Rhi::FormatEnum::R8_UNorm:
        return DXGI_FORMAT_R8_UNORM;
    default:
        assert(false && "Program should not reach this line");
        return DXGI_FORMAT_UNKNOWN;
    }
}

DXGI_FORMAT
GetDXGI_FORMAT(const Rhi::IndexType index_type)
{
    switch (index_type)
    {
    case Rhi::IndexType::Uint8:
        return DXGI_FORMAT_R8_UINT;
    case Rhi::IndexType::Uint16:
        return DXGI_FORMAT_R16_UINT;
    case Rhi::IndexType::Uint32:
        return DXGI_FORMAT_R32_UINT;
    default:
        assert(false && "Program should not reach this line");
        return DXGI_FORMAT_UNKNOWN;
    }
}

} // namespace DXA_NAME

#endif