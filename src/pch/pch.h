#pragma once

#if !defined(RHI_VKA) && !defined(RHI_DXA)
    #define RHI_VKA
    //#define RHI_DXA
#endif

#pragma warning(push, 0)
#ifndef CODE_ANALYSIS
    #ifdef RHI_VKA
        #ifdef HAS_RHI
            #define VKA_NAME RhiVka
        #else
            #define VKA_NAME Rhi
            #define HAS_RHI
        #endif
        #define USE_VKA
    #endif
    #ifdef RHI_DXA
        #ifdef HAS_RHI
            #define DXA_NAME RhiDxa
        #else
            #define DXA_NAME Rhi
            #define HAS_RHI
        #endif
        #define USE_DXA
    #endif

    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
    #ifdef USE_DXA
        #include <comdef.h>
        #include <shellapi.h>
        //
        #include <DirectXMath.h>
        #include <directx/d3d12.h>
        #include <directx/d3dx12.h>
        #include <dxgi1_6.h>
        //
        #include <D3D12MemAlloc.h>
    #endif

    #ifdef USE_VKA
        #define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
        #include <spirv_reflect.h>
        #include <vk_mem_alloc.h>
        #include <vulkan/vulkan.hpp>
    #endif

    // dxc
    #include <wrl/client.h>
    // dxcapi must be included after wrl/client
    #include <inc/dxcapi.h>
    #ifdef USE_DXA
        #include <inc/d3d12shader.h>
    #endif

    // glfw
    #include <GLFW/glfw3.h>
    #define GLFW_EXPOSE_NATIVE_WIN32
    #include <GLFW/glfw3native.h>

    // glm
    #include <glm/glm.hpp>
    #include <glm/gtc/packing.hpp>
    #include <glm/gtx/compatibility.hpp>
    #include <glm/gtx/string_cast.hpp>

    // imgui
    #include <backends/imgui_impl_glfw.h>
    #include <backends/imgui_impl_vulkan.h>
    #include <imgui.h>

    // assimp
    #include <assimp/Importer.hpp>
    #include <assimp/postprocess.h>
    #include <assimp/scene.h>

    // stb image
    #include <stb_image.h>

    // std library
    #include <array>
    #include <cassert>
    #include <chrono>
    #include <cstddef>
    #include <cstdint>
    #include <filesystem>
    #include <functional>
    #include <iostream>
    #include <map>
    #include <mutex>
    #include <numeric>
    #include <ostream>
    #include <set>
    #include <span>
    #include <sstream>
    #include <string>
    #include <thread>
    #include <variant>
    #include <vector>

#endif
#pragma warning(pop)