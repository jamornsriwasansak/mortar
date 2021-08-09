#pragma once

#include "common/logger.h"

#include "common/glfwhandler.h"
//
#include <DirectXMath.h>
#include <directx/d3d12.h>
#include <directx/d3dx12.h>
#include <dxgi1_6.h>
//
#pragma warning(push, 0)
#include "vk_mem_alloc.h"
#pragma warning(pop)

#ifndef DEFINE_ENUM_FLAG_OPERATORS
#define DEFINE_ENUM_FLAG_OPERATORS(ENUMTYPE)                                       \
    extern "C++"                                                                   \
    {                                                                              \
        inline ENUMTYPE                                                            \
        operator|(ENUMTYPE a, ENUMTYPE b)                                          \
        {                                                                          \
            return ENUMTYPE(((_ENUM_FLAG_SIZED_INTEGER<ENUMTYPE>::type)a) |        \
                            ((_ENUM_FLAG_SIZED_INTEGER<ENUMTYPE>::type)b));        \
        }                                                                          \
        inline ENUMTYPE &                                                          \
        operator|=(ENUMTYPE & a, ENUMTYPE b)                                       \
        {                                                                          \
            return (ENUMTYPE &)(((_ENUM_FLAG_SIZED_INTEGER<ENUMTYPE>::type &)a) |= \
                                ((_ENUM_FLAG_SIZED_INTEGER<ENUMTYPE>::type)b));    \
        }                                                                          \
        inline ENUMTYPE                                                            \
        operator&(ENUMTYPE a, ENUMTYPE b)                                          \
        {                                                                          \
            return ENUMTYPE(((_ENUM_FLAG_SIZED_INTEGER<ENUMTYPE>::type)a) &        \
                            ((_ENUM_FLAG_SIZED_INTEGER<ENUMTYPE>::type)b));        \
        }                                                                          \
        inline ENUMTYPE &                                                          \
        operator&=(ENUMTYPE & a, ENUMTYPE b)                                       \
        {                                                                          \
            return (ENUMTYPE &)(((_ENUM_FLAG_SIZED_INTEGER<ENUMTYPE>::type &)a) &= \
                                ((_ENUM_FLAG_SIZED_INTEGER<ENUMTYPE>::type)b));    \
        }                                                                          \
        inline ENUMTYPE                                                            \
        operator~(ENUMTYPE a)                                                      \
        {                                                                          \
            return ENUMTYPE(~((_ENUM_FLAG_SIZED_INTEGER<ENUMTYPE>::type)a));       \
        }                                                                          \
        inline ENUMTYPE                                                            \
        operator^(ENUMTYPE a, ENUMTYPE b)                                          \
        {                                                                          \
            return ENUMTYPE(((_ENUM_FLAG_SIZED_INTEGER<ENUMTYPE>::type)a) ^        \
                            ((_ENUM_FLAG_SIZED_INTEGER<ENUMTYPE>::type)b));        \
        }                                                                          \
        inline ENUMTYPE &                                                          \
        operator^=(ENUMTYPE & a, ENUMTYPE b)                                       \
        {                                                                          \
            return (ENUMTYPE &)(((_ENUM_FLAG_SIZED_INTEGER<ENUMTYPE>::type &)a) ^= \
                                ((_ENUM_FLAG_SIZED_INTEGER<ENUMTYPE>::type)b));    \
        }                                                                          \
    }
#endif

template <typename EnumType>
bool
HasFlag(const EnumType & value, const EnumType & must_have_flag)
{
    return (value & must_have_flag) == must_have_flag;
}

/*
 * Shader Stage Enum
 */

namespace Dxa
{
enum class ShaderStageEnum : uint16_t
{
    None         = 0,
    Vertex       = 1 << 0,
    Fragment     = 1 << 2,
    Geometry     = 1 << 3,
    Compute      = 1 << 4,
    RayGen       = 1 << 5,
    ClosestHit   = 1 << 6,
    AnyHit       = 1 << 7,
    Intersection = 1 << 8,
    Miss         = 1 << 9
};
DEFINE_ENUM_FLAG_OPERATORS(ShaderStageEnum);
} // namespace Dxa

namespace Vka
{
enum class ShaderStageEnum : uint32_t
{
    None         = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM,
    Vertex       = VK_SHADER_STAGE_VERTEX_BIT,
    Fragment     = VK_SHADER_STAGE_FRAGMENT_BIT,
    Geometry     = VK_SHADER_STAGE_GEOMETRY_BIT,
    Compute      = VK_SHADER_STAGE_COMPUTE_BIT,
    RayGen       = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
    ClosestHit   = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
    AnyHit       = VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
    Intersection = VK_SHADER_STAGE_INTERSECTION_BIT_KHR,
    Miss         = VK_SHADER_STAGE_MISS_BIT_KHR
};
DEFINE_ENUM_FLAG_OPERATORS(ShaderStageEnum);
} // namespace Vka

/*
 * Memory Usage
 */

namespace Dxa
{
enum class MemoryUsageEnum
{
    None,
    CpuOnly,
    GpuOnly,
    GpuToCpu,
    CpuToGpu
};
}

namespace Vka
{
enum class MemoryUsageEnum
{
    None     = VMA_MEMORY_USAGE_MAX_ENUM,
    CpuOnly  = VMA_MEMORY_USAGE_CPU_ONLY,
    GpuOnly  = VMA_MEMORY_USAGE_GPU_ONLY,
    GpuToCpu = VMA_MEMORY_USAGE_GPU_TO_CPU,
    CpuToGpu = VMA_MEMORY_USAGE_CPU_TO_GPU
};
}

/*
 * Buffer Usage
 */

namespace Dxa
{
enum class BufferUsageEnum : uint32_t
{
    None                        = 0,
    VertexBuffer                = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
    IndexBuffer                 = D3D12_RESOURCE_STATE_INDEX_BUFFER,
    ConstantBuffer              = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
    StorageBuffer               = D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
    TransferSrc                 = D3D12_RESOURCE_STATE_COPY_SOURCE,
    TransferDst                 = D3D12_RESOURCE_STATE_COPY_DEST,
    RayTracingAccelStructBuffer = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE
};
DEFINE_ENUM_FLAG_OPERATORS(BufferUsageEnum);
} // namespace Dxa

namespace Vka
{
enum class BufferUsageEnum : uint32_t
{
    None           = VK_BUFFER_USAGE_FLAG_BITS_MAX_ENUM,
    VertexBuffer   = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
    IndexBuffer    = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
    ConstantBuffer = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
    StorageBuffer  = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
    TransferSrc    = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    TransferDst    = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    RayTracingAccelStructBuffer = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
};
DEFINE_ENUM_FLAG_OPERATORS(BufferUsageEnum);
} // namespace Vka

/*
 * Texture Usage
 */

namespace Dxa
{
enum class TextureUsageEnum : uint16_t
{
    None            = D3D12_RESOURCE_FLAG_NONE,
    Sampled         = 0,
    ColorAttachment = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
    DepthAttachment = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
    StorageImage    = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
    TransferSrc     = 0,
    TransferDst     = 0
};
DEFINE_ENUM_FLAG_OPERATORS(TextureUsageEnum);
} // namespace Dxa

namespace Vka
{
enum class TextureUsageEnum : uint32_t
{
    None            = VK_IMAGE_USAGE_FLAG_BITS_MAX_ENUM,
    Sampled         = VK_IMAGE_USAGE_SAMPLED_BIT,
    ColorAttachment = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    DepthAttachment = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
    StorageImage    = VK_IMAGE_USAGE_STORAGE_BIT,
    TransferSrc     = VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
    TransferDst     = VK_IMAGE_USAGE_TRANSFER_DST_BIT
};
DEFINE_ENUM_FLAG_OPERATORS(TextureUsageEnum);
} // namespace Vka

/*
 * Texture Layout
 */

namespace Dxa
{
enum class TextureStateEnum : uint16_t
{
    None                     = 0,
    ColorAttachment          = D3D12_RESOURCE_STATE_RENDER_TARGET,
    DepthAttachment          = D3D12_RESOURCE_STATE_DEPTH_WRITE,
    FragmentShaderVisible    = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
    NonFragmentShaderVisible = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
    AllShaderVisible         = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
    Present                  = D3D12_RESOURCE_STATE_PRESENT,
    TransferSrc              = D3D12_RESOURCE_STATE_COPY_SOURCE,
    TransferDst              = D3D12_RESOURCE_STATE_COPY_DEST
};
} // namespace Dxa

namespace Vka
{
enum class TextureStateEnum : uint32_t
{
    None                     = VK_IMAGE_LAYOUT_UNDEFINED,
    ColorAttachment          = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    DepthAttachment          = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
    FragmentShaderVisible    = VK_IMAGE_LAYOUT_GENERAL, // VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    NonFragmentShaderVisible = VK_IMAGE_LAYOUT_GENERAL,
    AllShaderVisible         = VK_IMAGE_LAYOUT_GENERAL,
    Present                  = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    TransferSrc              = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    TransferDst              = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
};
} // namespace Vka


/*
 * Texture Format
 */

namespace Dxa
{
enum class FormatEnum : uint8_t
{
    None                = DXGI_FORMAT_UNKNOWN,
    Depth32_SFloat      = DXGI_FORMAT_D32_FLOAT,
    R32_UInt            = DXGI_FORMAT_R32_UINT,
    R11G11B10_UFloat    = DXGI_FORMAT_R11G11B10_FLOAT,
    R10G10B10A2_UNorm   = DXGI_FORMAT_R10G10B10A2_UNORM,
    R8_UNorm            = DXGI_FORMAT_R8_UNORM,
    R8G8B8A8_UNorm      = DXGI_FORMAT_R8G8B8A8_UNORM,
    R32G32B32_SFloat    = DXGI_FORMAT_R32G32B32_FLOAT,
    R32G32B32A32_SFloat = DXGI_FORMAT_R32G32B32A32_FLOAT
};
}

namespace Vka
{
enum class FormatEnum : uint32_t
{
    None                = VK_FORMAT_UNDEFINED,
    Depth32_SFloat      = VK_FORMAT_D32_SFLOAT,
    R32_UInt            = VK_FORMAT_R32_UINT,
    R11G11B10_UFloat    = VK_FORMAT_B10G11R11_UFLOAT_PACK32,
    R10G10B10A2_UNorm   = VK_FORMAT_A2R10G10B10_UNORM_PACK32,
    R8_UNorm            = VK_FORMAT_R8_UNORM,
    R8G8B8A8_UNorm      = VK_FORMAT_R8G8B8A8_UNORM,
    R32G32B32_SFloat    = VK_FORMAT_R32G32B32_SFLOAT,
    R32G32B32A32_SFloat = VK_FORMAT_R32G32B32A32_SFLOAT
};
}

namespace Dxa
{
enum class RayTracingGeometryFlag : uint8_t
{
    None   = D3D12_RAYTRACING_GEOMETRY_FLAG_NONE,
    Opaque = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE
};
}

namespace Vka
{
enum class RayTracingGeometryFlag : uint32_t
{
    None   = VK_GEOMETRY_FLAG_BITS_MAX_ENUM_KHR,
    Opaque = VK_GEOMETRY_OPAQUE_BIT_KHR
};
}

namespace Dxa
{
enum class IndexType : uint8_t
{
    None   = DXGI_FORMAT_UNKNOWN,
    Uint8  = DXGI_FORMAT_R8_UINT,
    Uint16 = DXGI_FORMAT_R16_UINT,
    Uint32 = DXGI_FORMAT_R32_UINT
};

inline size_t
GetSizeInBytes(IndexType it)
{
    switch (it)
    {
    case IndexType::Uint8:
        return sizeof(uint8_t);
    case IndexType::Uint16:
        return sizeof(uint16_t);
    case IndexType::Uint32:
        return sizeof(uint32_t);
    default:
        return 0;
    }
}
} // namespace Dxa

namespace Vka
{
enum class IndexType : uint32_t
{
    None   = VK_INDEX_TYPE_NONE_KHR,
    Uint8  = VK_INDEX_TYPE_UINT8_EXT,
    Uint16 = VK_INDEX_TYPE_UINT16,
    Uint32 = VK_INDEX_TYPE_UINT32
};

inline size_t
GetSizeInBytes(IndexType it)
{
    switch (it)
    {
    case IndexType::Uint8:
        return sizeof(uint8_t);
    case IndexType::Uint16:
        return sizeof(uint16_t);
    case IndexType::Uint32:
        return sizeof(uint32_t);
    default:
        return 0;
    }
}
} // namespace Vka

/*
 * Ray Tracing Accel
 */

namespace Dxa
{
enum class RayTracingAccelBuildFlag : uint8_t
{
    None            = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE,
    AllowCompaction = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_COMPACTION,
    AllowUpdate     = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE,
    FastTrace       = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE,
    FastBuild       = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD
};
}

namespace Vka
{
enum class RayTracingAccelBuildFlag : uint32_t
{
    None            = VK_BUILD_ACCELERATION_STRUCTURE_FLAG_BITS_MAX_ENUM_KHR,
    AllowCompaction = VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR,
    AllowUpdate     = VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR,
    FastTrace       = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
    FastBuild       = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR
};
}

/*
 * Attachment Load Op
 */

namespace Dxa
{
enum class LoadOp : uint8_t
{
    None,
    Load,
    Clear
};
}

namespace Vka
{
enum class LoadOp : uint8_t
{
    None  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
    Load  = VK_ATTACHMENT_LOAD_OP_LOAD,
    Clear = VK_ATTACHMENT_LOAD_OP_CLEAR
};
}

struct EnumHelper
{
    template <typename FormatEnum>
    static size_t
    GetSizeInBytesPerPixel(FormatEnum texture_type)
    {
        switch (texture_type)
        {
        case FormatEnum::None:
            return 0;
        case FormatEnum::R8_UNorm:
            return sizeof(uint8_t);
        case FormatEnum::R8G8B8A8_UNorm:
            return 4 * sizeof(uint8_t);
        default:
            Logger::Critical<true>(__FUNCTION__ " found unhandled texture type " + static_cast<int>(texture_type));
            return 0;
        }
    }
};


namespace Dxa
{
enum class CommandQueueType : uint8_t
{
    Graphics = D3D12_COMMAND_LIST_TYPE_DIRECT,
    Compute  = D3D12_COMMAND_LIST_TYPE_COMPUTE,
    Transfer = D3D12_COMMAND_LIST_TYPE_COPY
};
}

namespace Vka
{
enum class CommandQueueType : uint8_t
{
    Graphics = VK_QUEUE_GRAPHICS_BIT,
    Compute  = VK_QUEUE_COMPUTE_BIT,
    Transfer = VK_QUEUE_TRANSFER_BIT
};
}