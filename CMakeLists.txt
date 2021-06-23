# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required(VERSION 3.18)

project(mortar)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_TEST OFF CACHE BOOL "" FORCE)

set(DEPS_PATH ${CMAKE_CURRENT_SOURCE_DIR}/dep/ CACHE PATH "" FORCE)

# output directory
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/sandbox/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/sandbox/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/sandbox")

# GLFW 3
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    add_subdirectory(${DEPS_PATH}/glfw glfw_build)
    set(DEPS_LIBRARIES ${DEPS_LIBRARIES} glfw)

# directx
    add_subdirectory(${DEPS_PATH}/DirectX-Headers)
    set(DEPS_LIBRARIES ${DEPS_LIBRARIES} DirectX-Headers d3d12 dxgi dxguid d3dcompiler)
    file(COPY
         "${DEPS_PATH}/DirectXShaderCompilerPrebuilt-win/bin/x64/dxil.dll"
         "${DEPS_PATH}/DirectXShaderCompilerPrebuilt-win/bin/x64/dxcompiler.dll"
         DESTINATION
         ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

# vulkan
    find_package(Vulkan REQUIRED)
    include_directories(${Vulkan_INCLUDE_DIR})
    set(DEPS_LIBRARIES ${DEPS_LIBRARIES} ${Vulkan_LIBRARY})

# DirectXCompiler
# for compiling hlsl to SPIR-V
    # for linux (untested)
        #file (STRINGS "${DEPS_PATH}/DirectXShaderCompiler/utils/cmake-predefined-config-params" DXC_PREDEFINED_CONFIG)
        #add_subdirectory(${DEPS_PATH}/DirectXShaderCompiler DXC_PREDEFINED_CONFIG)
    # for windows
        include_directories(${DEPS_PATH}/DirectXShaderCompilerPrebuilt-win/inc)
        set(DXC_LIB ${DEPS_PATH}/DirectXShaderCompilerPrebuilt-win/lib/x64/dxcompiler.lib)
    set(DEPS_LIBRARIES ${DEPS_LIBRARIES} ${DXC_LIB})

# D3D12MemoryAllocator
    include_directories(SYSTEM ${DEPS_PATH}/D3D12MemoryAllocator/src)
    file(GLOB d3d12ma "${DEPS_PATH}/D3D12MemoryAllocator/src/D3D12MemAlloc.cpp")

# spirv-reflect
# for SPIR-V reflection
    include_directories(SYSTEM ${DEPS_PATH}/SPIRV-Reflect)
    file(GLOB spirv_reflect_src "${DEPS_PATH}/SPIRV-Reflect/spirv_reflect.c")

# GLM - math library
    include_directories(SYSTEM ${DEPS_PATH}/glm)

# stb - stb images and etc.
    include_directories(SYSTEM ${DEPS_PATH}/stb)

# assimp
    set(ASSIMP_NO_EXPORT ON CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
    set(ASSIMP_INJECT_DEBUG_POSTFIX OFF CACHE BOOL "" FORCE)
    # mesh format
    set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_OBJ_IMPORTER ON CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_FBX_IMPORTER ON CACHE BOOL "" FORCE)
    set(ASSIMP_INCLUDE_DIRS ${CMAKE_BINARY_DIR}/assimp_build/include)
    add_subdirectory(${DEPS_PATH}/assimp assimp_build)
    set(DEPS_LIBRARIES ${DEPS_LIBRARIES} assimp)

# spdlog
# spdlog - Logger
    include_directories(SYSTEM ${DEPS_PATH}/spdlog/include)

# vma
    include_directories(SYSTEM ${DEPS_PATH}/VulkanMemoryAllocator/include)

# NVIDIA aftermath
# TODO:: make an option to toggle this on or off
    # for windows x64
    #include_directories(SYSTEM $ENV{NSIGHT_AFTERMATH_SDK}/include)
    #include_directories(SYSTEM ${DEPS_PATH}/nsightaftermathhelper/include)
    #file(GLOB GFSDK_Aftermath_src "${DEPS_PATH}/nsightaftermathhelper/include/*.cpp")
    #set(NVIDIA_AFTERMATH_LIB $ENV{NSIGHT_AFTERMATH_SDK}/lib/x64/GFSDK_Aftermath_Lib.x64.lib)
    #set(DEPS_LIBRARIES ${DEPS_LIBRARIES} ${NVIDIA_AFTERMATH_LIB})

# aftercrash
    #include_directories(SYSTEM "${DEPS_PATH}/aftercrash/include")
    #file(GLOB aftercrash_src "${DEPS_PATH}/aftercrash/src/D3d12AfterCrash.cpp" "${DEPS_PATH}/aftercrash/src/vulkanaftercrash.cpp")

# mortar
    # find all sources
    include_directories(src)
    file(GLOB_RECURSE mortar_src
         "src/*.h"
         "src/*.hpp"
         "src/*.cpp"
         "src/*.cc")

    # C++ stuffs
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DAVX2")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG")
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    add_compile_definitions(NOMINMAX)

    if(MSVC)
        add_compile_options(/W4)
    else()
        add_compile_options(-Wall -Wextra -pedantic)
    endif()

    # make sure compiler does not generate unnecessary warning from third party
    set_source_files_properties(${d3d12ma}
                                PROPERTIES
                                COMPILER_FLAGS "-w")

    # generating executable
    add_executable(mortar ${mortar_src} ${spirv_reflect_src} ${d3d12ma} ${aftercrash_src} ${GFSDK_Aftermath_src} "src/render/passes/directlight/bindlessobjectable.h")
    target_link_libraries(mortar PUBLIC ${DEPS_LIBRARIES})

set(CMAKE_VERBOSE_MAKEFILE ON)
