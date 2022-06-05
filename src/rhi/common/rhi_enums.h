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

/*
 * Shader Stage Enum
 */

namespace Rhi
{

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

enum class ShaderStageEnum
{
    None,
    Vertex,
    Fragment,
    Geometry,
    Compute,
    RayGen,
    ClosestHit,
    AnyHit,
    Intersection,
    Miss
};

enum class MemoryUsageEnum
{
    CpuOnly,
    GpuOnly,
    GpuToCpu,
    CpuToGpu
};

enum class BufferUsageEnum
{
    Common                           = 1 << 0,
    VertexBuffer                     = 1 << 1,
    IndexBuffer                      = 1 << 2,
    RayTracingAccelStructBufferInput = 1 << 3,
    ConstantBuffer                   = 1 << 4,
    StorageBuffer                    = 1 << 5,
    TransferSrc                      = 1 << 6,
    TransferDst                      = 1 << 7,
    RayTracingAccelStructBuffer      = 1 << 8
};
DEFINE_ENUM_FLAG_OPERATORS(BufferUsageEnum);

enum class TextureUsageEnum
{
    Common          = 1 << 0,
    ColorAttachment = 1 << 1,
    DepthAttachment = 1 << 2,
    StorageImage    = 1 << 3,
    TransferSrc     = 1 << 4,
    TransferDst     = 1 << 5
};
DEFINE_ENUM_FLAG_OPERATORS(TextureUsageEnum);

enum class TextureStateEnum
{
    ColorAttachment,
    DepthAttachment,
    ReadOnly,
    ReadWrite,
    Present,
    TransferSrc,
    TransferDst
};

enum class FormatEnum
{
    Depth32_SFloat,
    R10G10B10A2_UNorm,
    R11G11B10_UFloat,
    R16G16B16A16_SNorm,
    R16G16_SNorm,
    R16_SFloat,
    R16_UInt,
    R16_UNorm,
    R32G32B32A32_SFloat,
    R32G32B32_SFloat,
    R32_SFloat,
    R32_UInt,
    R8G8B8A8_UNorm_Srgb,
    R8G8B8A8_UNorm,
    R8_UNorm
};

enum class IndexType
{
    Uint8,
    Uint16,
    Uint32
};

} // namespace Rhi

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
    if constexpr (sizeof(Index) == 1)
    {
        return IndexType::Uint8;
    }
    else if constexpr (sizeof(Index) == 2)
    {
        return IndexType::Uint16;
    }
    else if constexpr (sizeof(Index) == 4)
    {
        return IndexType::Uint32;
    }

    throw std::runtime_error("unknown type");
    return IndexType::Uint32;
}

template <typename Format>
constexpr inline FormatEnum
GetVertexType()
{
    if constexpr (std::is_same<Format, float3>::value)
    {
        return FormatEnum::R32G32B32_SFloat;
    }

    throw std::runtime_error("unknown type");
    return FormatEnum::R32G32B32_SFloat;
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
        case FormatEnum::R8_UNorm:
            return sizeof(uint8_t);
        case FormatEnum::R8G8B8A8_UNorm:
        case FormatEnum::R8G8B8A8_UNorm_Srgb:
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