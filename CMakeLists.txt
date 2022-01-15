cmake_minimum_required(VERSION 3.19.0)
project(mortar VERSION 0.1.0)

# vulkan
    find_package(Vulkan REQUIRED)

# USD (Universal Scene Description)
    # set pxr DIR
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(pxr_DIR "C:/USD/debug")
    else()
        set(pxr_DIR "C:/USD/release")
    endif()
    find_package(pxr REQUIRED CONFIG)

add_subdirectory(dep)
add_subdirectory(src)