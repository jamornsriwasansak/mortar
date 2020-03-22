# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required(VERSION 3.8)

project(mortar)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)

# vulkan
	find_package(Vulkan)
	include_directories(${Vulkan_INCLUDE_DIR})

# glslang
	set(SKIP_GLSLANG_INSTALL ON CACHE BOOL "" FORCE)
	set(ENABLE_CTEST OFF CACHE BOOL "" FORCE)
	set(ENABLE_HLSL OFF CACHE BOOL "" FORCE)
	set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "" FORCE)
	add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/dependencies/glslang)

# GLFW 3
	set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
	set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
	set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
	set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
	add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/dependencies/glfw)

# GLM
	include_directories(${CMAKE_CURRENT_SOURCE_DIR}/dependencies/glm)

# stb
	include_directories(${CMAKE_CURRENT_SOURCE_DIR}/dependencies/stb)

# tinyobjloader
	include_directories(${CMAKE_CURRENT_SOURCE_DIR}/dependencies/tinyobjloader)

# mortar
	# C++ stuffs
	set(CMAKE_CXX_STANDARD 17)
	set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DAVX2")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")
	add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
	add_compile_definitions(NOMINMAX)

	# output directory
	set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/sandbox/lib")
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/sandbox/lib")
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/sandbox")

	# find all sources
	file(GLOB_RECURSE mortar_src
		 "src/*.h"
		 "src/*.hpp"
		 "src/*.cpp"
		 "src/*.cc")

	# make sure compiler does not generate unnecessary warning from third party
	set_source_files_properties(${mortar_src}
								PROPERTIES)

	# options for generating executable
	include_directories(src)
	add_executable(mortar ${mortar_src})
	target_link_libraries(mortar SPIRV glslang glfw ${Vulkan_LIBRARY})

	# copy shaders
	add_custom_target(mortar_shaders_copy)
	add_custom_command(TARGET mortar_shaders_copy POST_BUILD
					   COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/ ${CMAKE_CURRENT_SOURCE_DIR}/sandbox/shaders/
					   COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/src/app_shader_shared/shared.glsl.h ${CMAKE_CURRENT_SOURCE_DIR}/sandbox/shaders/)
	add_custom_target(mortar_resource_copy)
	add_custom_command(TARGET mortar_resource_copy POST_BUILD
					   COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/resource/ ${CMAKE_CURRENT_SOURCE_DIR}/sandbox/resource/)
	add_dependencies(mortar mortar_shaders_copy mortar_resource_copy)

	#file(GLOB_RECURSE mortar_shader_src
	#	 "src/*.vert"
	#	 "src/*.frag",
	#	 "src/*.glsl",
	#	 "src/*.rchit",
	#	 "src/*.rmiss",
	#	 "src/*.rgen")

	#add_custom_target(mortar_shaders_copy)
	#foreach(shader IN LISTS mortar_shader_src)
	#	add_custom_command(TARGET mortar_shaders_copy POST_BUILD
	#					   COMMAND ${CMAKE_COMMAND} -E copy_if_different ${shader} ${CMAKE_CURRENT_SOURCE_DIR}/sandbox/)
	#	add_dependencies(mortar mortar_shaders_copy)
	#endforeach()

set(CMAKE_VERBOSE_MAKEFILE ON)
