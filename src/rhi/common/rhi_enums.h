#pragma once

#include "pch/pch.h"

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

template <typename EnumType>
bool
HasOnlyFlag(const EnumType & value, const EnumType & flag)
{
    return value == flag;
}

/*
 * Shader Stage Enum
 */

#ifdef USE_DXA
namespace DXA_NAME
{
enum class ShaderStageEnum : uint32_t
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
} // namespace DXA_NAME
#endif

#ifdef USE_VKA
namespace VKA_NAME
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
} // namespace VKA_NAME
#endif

/*
 * Memory Usage
 */

#ifdef USE_DXA
namespace DXA_NAME
{
enum class MemoryUsageEnum : uint32_t
{
    None,
    CpuOnly,
    GpuOnly,
    GpuToCpu,
    CpuToGpu
};
}
#endif

#ifdef USE_VKA
namespace VKA_NAME
{
enum class MemoryUsageEnum : uint32_t
{
    None     = VMA_MEMORY_USAGE_MAX_ENUM,
    CpuOnly  = VMA_MEMORY_USAGE_CPU_ONLY,
    GpuOnly  = VMA_MEMORY_USAGE_GPU_ONLY,
    GpuToCpu = VMA_MEMORY_USAGE_GPU_TO_CPU,
    CpuToGpu = VMA_MEMORY_USAGE_CPU_TO_GPU
};
}
#endif

/*
 * Buffer Usage
 */
#ifdef USE_DXA
namespace DXA_NAME
{
enum class BufferUsageEnum : uint32_t
{
    None                             = 0,
    VertexBuffer                     = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
    IndexBuffer                      = D3D12_RESOURCE_STATE_INDEX_BUFFER,
    RayTracingAccelStructBufferInput = 0,
    ConstantBuffer                   = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
    StorageBuffer                    = D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
    TransferSrc                      = D3D12_RESOURCE_STATE_COPY_SOURCE,
    TransferDst                      = D3D12_RESOURCE_STATE_COPY_DEST,
    RayTracingAccelStructBuffer      = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE
};
DEFINE_ENUM_FLAG_OPERATORS(BufferUsageEnum);
} // namespace DXA_NAME
#endif

#ifdef USE_VKA
namespace VKA_NAME
{
enum class BufferUsageEnum : uint32_t
{
    None                             = VK_BUFFER_USAGE_FLAG_BITS_MAX_ENUM,
    VertexBuffer                     = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
    IndexBuffer                      = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
    RayTracingAccelStructBufferInput = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
    ConstantBuffer                   = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
    StorageBuffer                    = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
    TransferSrc                      = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    TransferDst                      = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    RayTracingAccelStructBuffer = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
};
DEFINE_ENUM_FLAG_OPERATORS(BufferUsageEnum);
} // namespace VKA_NAME
#endif

/*
 * Texture Usage
 */

#ifdef USE_DXA
namespace DXA_NAME
{
enum class TextureUsageEnum : uint32_t
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
} // namespace DXA_NAME
#endif

#ifdef USE_VKA
namespace VKA_NAME
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
} // namespace VKA_NAME
#endif

/*
 * Texture Layout
 */

#ifdef USE_DXA
namespace DXA_NAME
{
enum class TextureStateEnum : uint32_t
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
} // namespace DXA_NAME
#endif

#ifdef USE_VKA
namespace VKA_NAME
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
} // namespace VKA_NAME
#endif


/*
 * Texture Format
 */

#ifdef USE_DXA
namespace DXA_NAME
{
enum class FormatEnum : uint32_t
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
} // namespace DXA_NAME
#endif

#ifdef USE_VKA
namespace VKA_NAME
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
} // namespace VKA_NAME
#endif

namespace Rhi
{
constexpr inline size_t
GetSizeInBytes(const FormatEnum & format_enum)
{
    switch (format_enum)
    {
    case FormatEnum::R32G32B32_SFloat:
        return sizeof(float) * 3;
    case FormatEnum::R32G32B32A32_SFloat:
        return sizeof(float) * 4;
    default:
        assert(false); // unhandled case
        return 0;
    }
}
} // namespace Rhi

#ifdef USE_DXA
namespace DXA_NAME
{
enum class RayTracingGeometryFlag : uint32_t
{
    None   = D3D12_RAYTRACING_GEOMETRY_FLAG_NONE,
    Opaque = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE
};
}
#endif

#ifdef USE_VKA
namespace VKA_NAME
{
enum class RayTracingGeometryFlag : uint32_t
{
    None   = VK_GEOMETRY_FLAG_BITS_MAX_ENUM_KHR,
    Opaque = VK_GEOMETRY_OPAQUE_BIT_KHR
};
}
#endif

#ifdef USE_DXA
namespace DXA_NAME
{
enum class IndexType : uint32_t
{
    None   = DXGI_FORMAT_UNKNOWN,
    Uint8  = DXGI_FORMAT_R8_UINT,
    Uint16 = DXGI_FORMAT_R16_UINT,
    Uint32 = DXGI_FORMAT_R32_UINT
};
} // namespace DXA_NAME
#endif

#ifdef USE_VKA
namespace VKA_NAME
{
enum class IndexType : uint32_t
{
    None   = VK_INDEX_TYPE_NONE_KHR,
    Uint8  = VK_INDEX_TYPE_UINT8_EXT,
    Uint16 = VK_INDEX_TYPE_UINT16,
    Uint32 = VK_INDEX_TYPE_UINT32
};
} // namespace VKA_NAME
#endif

namespace Rhi
{
constexpr inline size_t
GetSizeInBytes(const IndexType it)
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

template <typename Index>
constexpr inline IndexType
GetIndexType()
{
    if constexpr (std::is_same<Index, uint8_t>::value)
    {
        return IndexType::Uint8;
    }
    else if constexpr (std::is_same<Index, uint16_t>::value)
    {
        return IndexType::Uint16;
    }
    else if constexpr (std::is_same<Index, uint32_t>::value)
    {
        return IndexType::Uint32;
    }
    else
    {
        static_assert(false, "unknown index type");
    }
}

template <typename Format>
constexpr inline FormatEnum
GetVertexType()
{
    if constexpr (std::is_same<Format, float3>::value)
    {
        return FormatEnum::R32G32B32_SFloat;
    }
    else
    {
        static_assert(false, "unknown index type");
    }
}
} // namespace Rhi

/*
 * Ray Tracing Accel
 */

#ifdef USE_DXA
namespace DXA_NAME
{
enum class RayTracingAccelBuildFlag : uint32_t
{
    None            = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE,
    AllowCompaction = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_COMPACTION,
    AllowUpdate     = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE,
    FastTrace       = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE,
    FastBuild       = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD
};
}
#endif

#ifdef USE_VKA
namespace VKA_NAME
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
#endif

/*
 * Attachment Load Op
 */

#ifdef USE_DXA
namespace DXA_NAME
{
enum class LoadOp : uint32_t
{
    None,
    Load,
    Clear
};
}
#endif

#ifdef USE_VKA
namespace VKA_NAME
{
enum class LoadOp : uint32_t
{
    None  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
    Load  = VK_ATTACHMENT_LOAD_OP_LOAD,
    Clear = VK_ATTACHMENT_LOAD_OP_CLEAR
};
}
#endif

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

#ifdef USE_DXA
namespace DXA_NAME
{
enum class QueueType : uint32_t
{
    Graphics = D3D12_COMMAND_LIST_TYPE_DIRECT,
    Compute  = D3D12_COMMAND_LIST_TYPE_COMPUTE,
    Transfer = D3D12_COMMAND_LIST_TYPE_COPY
};
}
#endif

#ifdef USE_VKA
namespace VKA_NAME
{
enum class QueueType : uint32_t
{
    Graphics = VK_QUEUE_GRAPHICS_BIT,
    Compute  = VK_QUEUE_COMPUTE_BIT,
    Transfer = VK_QUEUE_TRANSFER_BIT
};
}
#endif

#ifdef USE_DXA
namespace DXA_NAME
{
enum class RayTracingBuildHint : uint32_t
{
    NonDeformable = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE,
    Deformable    = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD |
                 D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE,
    Hero = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE | D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE
};
}
#endif

#ifdef USE_VKA
namespace VKA_NAME
{
enum class RayTracingBuildHint : uint32_t
{
    NonDeformable = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
    Deformable    = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR,
    Hero          = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR
};
}
#endif

#ifdef USE_DXA
namespace DXA_NAME
{
enum class QueryType : uint32_t
{
    Occlusion         = D3D12_QUERY_HEAP_TYPE_OCCLUSION,
    PipelineStatistic = D3D12_QUERY_HEAP_TYPE_PIPELINE_STATISTICS,
    Timestamp         = D3D12_QUERY_HEAP_TYPE_TIMESTAMP
};
}
#endif

#ifdef USE_VKA
namespace VKA_NAME
{
enum class QueryType : uint32_t
{
    Occlusion          = VK_QUERY_TYPE_OCCLUSION,
    PipelineStatistics = VK_QUERY_TYPE_PIPELINE_STATISTICS,
    Timestamp          = VK_QUERY_TYPE_TIMESTAMP
};
}
#endif