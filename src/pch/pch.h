#pragma once

#pragma warning(push, 0)
#ifndef CODE_ANALYSIS

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#if 0
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

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>

// vulkan memory allocator
#include <vk_mem_alloc.h>

// dxc
#include <wrl/client.h>
//
#include <inc/dxcapi.h>
#include <spirv_reflect.h>

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

// stb image
#include <stb_image.h>

// std library
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <map>
#include <mutex>
#include <numeric>
#include <optional>
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