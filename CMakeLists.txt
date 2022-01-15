cmake_minimum_required(VERSION 3.19.0)
project(mortar VERSION 0.1.0)

# vulkan
    find_package(Vulkan REQUIRED)

# USD (Universal Scene Description)
    find_package(pxr REQUIRED)

add_subdirectory(dep)
add_subdirectory(src)