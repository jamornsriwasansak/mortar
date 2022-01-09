#pragma once

#include "pch/pch.h"

#ifdef USE_VKA

    #include "rhi/common/rhi_enums.h"

namespace VKA_NAME
{
vk::ShaderStageFlagBits
GetVkShaderStageFlagBits(const Rhi::ShaderStageEnum shader_stage_enum)
{
    switch (shader_stage_enum)
    {
    case Rhi::ShaderStageEnum::Vertex:
        return vk::ShaderStageFlagBits::eVertex;
    case Rhi::ShaderStageEnum::Fragment:
        return vk::ShaderStageFlagBits::eFragment;
    case Rhi::ShaderStageEnum::Geometry:
        return vk::ShaderStageFlagBits::eGeometry;
    case Rhi::ShaderStageEnum::Compute:
        return vk::ShaderStageFlagBits::eCompute;
    case Rhi::ShaderStageEnum::RayGen:
        return vk::ShaderStageFlagBits::eRaygenKHR;
    case Rhi::ShaderStageEnum::ClosestHit:
        return vk::ShaderStageFlagBits::eClosestHitKHR;
    case Rhi::ShaderStageEnum::AnyHit:
        return vk::ShaderStageFlagBits::eAnyHitKHR;
    case Rhi::ShaderStageEnum::Intersection:
        return vk::ShaderStageFlagBits::eIntersectionKHR;
    case Rhi::ShaderStageEnum::Miss:
        return vk::ShaderStageFlagBits::eMissKHR;
    default:
        vk::ShaderStageFlagBits result;
        assert(false && "Program should not reach this line");
        return result;
    }
}

VmaMemoryUsage
GetVmaMemoryUsage(const Rhi::MemoryUsageEnum memory_usage)
{
    switch (memory_usage)
    {
    case Rhi::MemoryUsageEnum::CpuOnly:
        return VMA_MEMORY_USAGE_CPU_ONLY;
    case Rhi::MemoryUsageEnum::GpuOnly:
        return VMA_MEMORY_USAGE_GPU_ONLY;
    case Rhi::MemoryUsageEnum::GpuToCpu:
        return VMA_MEMORY_USAGE_GPU_TO_CPU;
    case Rhi::MemoryUsageEnum::CpuToGpu:
        return VMA_MEMORY_USAGE_CPU_TO_GPU;
    default:
        assert(false && "Program should not reach this line");
        return VMA_MEMORY_USAGE_MAX_ENUM;
    }
}

vk::BufferUsageFlags
GetVkBufferUsageFlags(const Rhi::BufferUsageEnum buffer_usage)
{
    vk::BufferUsageFlags result;
    if (Rhi::HasFlag(buffer_usage, Rhi::BufferUsageEnum::VertexBuffer))
        result = result | vk::BufferUsageFlagBits::eVertexBuffer;
    if (Rhi::HasFlag(buffer_usage, Rhi::BufferUsageEnum::IndexBuffer))
        result = result | vk::BufferUsageFlagBits::eIndexBuffer;
    if (Rhi::HasFlag(buffer_usage, Rhi::BufferUsageEnum::RayTracingAccelStructBufferInput))
        result = result | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR;
    if (Rhi::HasFlag(buffer_usage, Rhi::BufferUsageEnum::ConstantBuffer))
        result = result | vk::BufferUsageFlagBits::eUniformBuffer;
    if (Rhi::HasFlag(buffer_usage, Rhi::BufferUsageEnum::StorageBuffer))
        result = result | vk::BufferUsageFlagBits::eStorageBuffer;
    if (Rhi::HasFlag(buffer_usage, Rhi::BufferUsageEnum::TransferSrc))
        result = result | vk::BufferUsageFlagBits::eTransferSrc;
    if (Rhi::HasFlag(buffer_usage, Rhi::BufferUsageEnum::TransferDst))
        result = result | vk::BufferUsageFlagBits::eTransferDst;
    if (Rhi::HasFlag(buffer_usage, Rhi::BufferUsageEnum::RayTracingAccelStructBuffer))
        result = result | vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR |
                 vk::BufferUsageFlagBits::eShaderDeviceAddress;
    return result;
}

vk::ImageUsageFlags
GetVkUsageFlags(const Rhi::TextureUsageEnum texture_usage)
{
    vk::ImageUsageFlags result;
    if (Rhi::HasFlag(texture_usage, Rhi::TextureUsageEnum::ColorAttachment))
        result = result | vk::ImageUsageFlagBits::eColorAttachment;
    if (Rhi::HasFlag(texture_usage, Rhi::TextureUsageEnum::DepthAttachment))
        result = result | vk::ImageUsageFlagBits::eDepthStencilAttachment;
    if (Rhi::HasFlag(texture_usage, Rhi::TextureUsageEnum::StorageImage))
        result = result | vk::ImageUsageFlagBits::eStorage;
    if (Rhi::HasFlag(texture_usage, Rhi::TextureUsageEnum::TransferSrc))
        result = result | vk::ImageUsageFlagBits::eTransferSrc;
    if (Rhi::HasFlag(texture_usage, Rhi::TextureUsageEnum::TransferDst))
        result = result | vk::ImageUsageFlagBits::eTransferDst;
    return result;
}

vk::ImageLayout
GetVkImageLayout(const Rhi::TextureStateEnum texture_state)
{
    switch (texture_state)
    {
    case Rhi::TextureStateEnum::ColorAttachment:
        return vk::ImageLayout::eColorAttachmentOptimal;
    case Rhi::TextureStateEnum::DepthAttachment:
        return vk::ImageLayout::eDepthAttachmentOptimal;
    case Rhi::TextureStateEnum::ReadOnly:
        return vk::ImageLayout::eShaderReadOnlyOptimal;
    case Rhi::TextureStateEnum::ReadWrite:
        return vk::ImageLayout::eGeneral;
    case Rhi::TextureStateEnum::Present:
        return vk::ImageLayout::ePresentSrcKHR;
    case Rhi::TextureStateEnum::TransferSrc:
        return vk::ImageLayout::eTransferSrcOptimal;
    case Rhi::TextureStateEnum::TransferDst:
        return vk::ImageLayout::eTransferDstOptimal;
    default:
        assert(false && "Program should not reach this line");
        return vk::ImageLayout::eUndefined;
    }
}

vk::Format
GetVkFormat(const Rhi::FormatEnum format_enum)
{
    switch (format_enum)
    {
    case Rhi::FormatEnum::Depth32_SFloat:
        return vk::Format::eD32Sfloat;
    case Rhi::FormatEnum::R8G8B8A8_UNorm:
        return vk::Format::eR8G8B8A8Unorm;
    case Rhi::FormatEnum::R8G8B8A8_UNorm_Srgb:
        return vk::Format::eR8G8B8A8Srgb;
    case Rhi::FormatEnum::R8_UNorm:
        return vk::Format::eR8Unorm;
    case Rhi::FormatEnum::R10G10B10A2_UNorm:
        return vk::Format::eA2R10G10B10UnormPack32;
    case Rhi::FormatEnum::R11G11B10_UFloat:
        return vk::Format::eB10G11R11UfloatPack32;
    case Rhi::FormatEnum::R16G16B16A16_SNorm:
        return vk::Format::eR16G16B16A16Snorm;
    case Rhi::FormatEnum::R16G16_SNorm:
        return vk::Format::eR16G16Snorm;
    case Rhi::FormatEnum::R16_SFloat:
        return vk::Format::eR16Sfloat;
    case Rhi::FormatEnum::R16_UInt:
        return vk::Format::eR16Uint;
    case Rhi::FormatEnum::R16_UNorm:
        return vk::Format::eR16Unorm;
    case Rhi::FormatEnum::R32G32B32A32_SFloat:
        return vk::Format::eR32G32B32A32Sfloat;
    case Rhi::FormatEnum::R32G32B32_SFloat:
        return vk::Format::eR32G32B32Sfloat;
    case Rhi::FormatEnum::R32_SFloat:
        return vk::Format::eR32Sfloat;
    case Rhi::FormatEnum::R32_UInt:
        return vk::Format::eR32Uint;
    default:
        assert(false && "Program should not reach this line");
        return vk::Format::eUndefined;
    }
}

vk::IndexType
GetVkIndexType(const Rhi::IndexType index_type)
{
    switch (index_type)
    {
    case Rhi::IndexType::Uint32:
        return vk::IndexType::eUint32;
    case Rhi::IndexType::Uint16:
        return vk::IndexType::eUint16;
    case Rhi::IndexType::Uint8:
        return vk::IndexType::eUint8EXT;
    default:
        assert(false && "Program should not reach this line");
        return vk::IndexType::eNoneKHR;
    }
}
} // namespace VKA_NAME
#endif
