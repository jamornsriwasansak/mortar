# GLM - math library
    set(GLM_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/glm PARENT_SCOPE)

# GLFW 3
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    add_subdirectory(glfw)

# directx
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/DirectX-Headers)
    #file(COPY
         #"${DEPS_PATH}/DirectXShaderCompilerPrebuilt-win/bin/x64/dxil.dll"
         #"${DEPS_PATH}/DirectXShaderCompilerPrebuilt-win/bin/x64/dxcompiler.dll"
         #DESTINATION
         #${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

# vulkan
    find_package(Vulkan REQUIRED)

# vulkan
    #find_package(Vulkan REQUIRED)

# vulkan memory allocator
    set(VMA_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/VulkanMemoryAllocator/include PARENT_SCOPE)

# DirectXCompiler
    # for linux (untested)
        #file (STRINGS "${DEPS_PATH}/DirectXShaderCompiler/utils/cmake-predefined-config-params" DXC_PREDEFINED_CONFIG)
        #add_subdirectory(${DEPS_PATH}/DirectXShaderCompiler DXC_PREDEFINED_CONFIG)
    # for windows
        set(DXC_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/DirectXShaderCompilerPrebuilt-win PARENT_SCOPE)
        set(DXC_LIB ${CMAKE_CURRENT_SOURCE_DIR}/DirectXShaderCompilerPrebuilt-win/lib/x64/dxcompiler.lib PARENT_SCOPE)

# spirv reflect
    set(SPIRV_REFLECT_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/SPIRV-Reflect PARENT_SCOPE)
    file(GLOB TMP_SPIRV_REFLECT_SOURCE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/SPIRV-Reflect/spirv_reflect.c" PARENT_SCOPE)
    set(SPIRV_REFLECT_SOURCE_DIRS ${TMP_SPIRV_REFLECT_SOURCE_DIRS} PARENT_SCOPE)

# assimp
    set(ASSIMP_NO_EXPORT ON CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
    set(ASSIMP_INJECT_DEBUG_POSTFIX OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    # mesh format
    set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_OBJ_IMPORTER ON CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_FBX_IMPORTER ON CACHE BOOL "" FORCE)
    set(ASSIMP_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/assimp/include PARENT_SCOPE)
    set(ASSIMP_CONFIG_INCLUDE_DIRS ${CMAKE_BINARY_DIR}/dep/assimp/include PARENT_SCOPE)
    add_subdirectory(assimp)

# spdlog
    set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_BENCH OFF CACHE BOOL "" FORCE)
    set(SPDLOG_INSTALL OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(SPDLOG_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/spdlog/include PARENT_SCOPE)
    add_subdirectory(spdlog)

# stbimage
    set(STBIMAGE_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/stb PARENT_SCOPE)

# imgui
    set(IMGUI_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/imgui/ PARENT_SCOPE)
    file(GLOB TMP_IMGUI_SOURCE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/imgui/*.cpp" PARENT_SCOPE)
    file(GLOB TMP_IMGUI_BACKEND_SOURCE_DIRS 
        "${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends/imgui_impl_dx12.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends/imgui_impl_vulkan.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends/imgui_impl_glfw.cpp"
        PARENT_SCOPE)
    set(IMGUI_SOURCE_DIRS ${TMP_IMGUI_SOURCE_DIRS} PARENT_SCOPE)
    set(IMGUI_BACKEND_SOURCE_DIRS ${TMP_IMGUI_BACKEND_SOURCE_DIRS} PARENT_SCOPE)