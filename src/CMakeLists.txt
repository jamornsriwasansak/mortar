add_executable(mortar
    main.cpp
    pch/stb_image.cpp
    pch/vk_dispatch_loader_dynamic.cpp
    pch/vma.cpp
    ${SPIRV_REFLECT_SOURCE_DIRS}
    ${IMGUI_SOURCE_DIRS}
    ${IMGUI_BACKEND_SOURCE_DIRS}
    ${D3D12_MEM_ALLOCATOR_SOURCE_DIRS})

target_link_libraries(
    mortar PRIVATE
    glfw
    assimp
    ${DXC_LIB}
    ${Vulkan_LIBRARY}
    DirectX-Headers d3d12 dxgi dxguid d3dcompiler)

target_include_directories(
    mortar SYSTEM PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${IMGUI_INCLUDE_DIRS}
    ${SPDLOG_INCLUDE_DIRS}
    ${VULKAN_INCLUDE_DIRS}
    ${DXC_INCLUDE_DIRS}
    ${GLM_INCLUDE_DIRS}
    ${VMA_INCLUDE_DIRS}
    ${Vulkan_INCLUDE_DIR}
    ${SPIRV_REFLECT_INCLUDE_DIRS}
    ${STBIMAGE_INCLUDE_DIRS}
    ${D3D12_MEM_ALLOCATOR_INCLUDE_DIRS})

set_target_properties(
    mortar
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/../bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/../bin")

add_custom_command(
    TARGET mortar POST_BUILD COMMAND
    ${CMAKE_COMMAND} -E copy ${DXC_BIN} "${CMAKE_CURRENT_SOURCE_DIR}/../bin")

target_compile_features(mortar PRIVATE cxx_std_20)
target_compile_definitions(mortar PRIVATE IMGUI_USER_CONFIG="pch/imgui_config.h")
target_compile_definitions(mortar PRIVATE __STDC_WANT_SECURE_LIB__=1 _SCL_SECURE_NO_WARNINGS NOMINMAX)
target_precompile_headers(mortar PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/pch/pch.h>")
if (MSVC)
    #target_compile_options(mortar PRIVATE /W4 /WX)
    target_compile_options(mortar PRIVATE /W4 /analyze)
else()
    target_compile_options(mortar PRIVATE -Wall -Wextra -pedantic -Werror)
endif()