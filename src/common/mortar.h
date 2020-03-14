#pragma once

#include <iostream>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <tuple>
#include <variant>
#include <vector>

#include <vulkan/vulkan.hpp>

#define GLM_FORCE_RADIANS
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/string_cast.hpp"
using namespace glm;

#include "app_shader_shared/shared.glsl.h"

const int Width = 1280;
const int Height = 720;
const uvec2 ImageExtent = uvec2(Width, Height);
const uint32_t VkVersion = 120;

const bool UseValidationLayer = true;
const vk::Format RgbaUnormFormat = vk::Format::eB8G8R8A8Unorm;
const vk::Format DepthFormat = vk::Format::eD32Sfloat;
const vk::ColorSpaceKHR ColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
const vk::PresentModeKHR PresentMode = vk::PresentModeKHR::eMailbox;
const vk::Extent2D Extent = vk::Extent2D(Width, Height);

void
THROW(const std::string & message)
{
	std::cout << message << std::endl;
	throw std::runtime_error(message);
}

template <typename T>
void
THROW_ASSERT(const T & condition,
			 const std::string & message)
{
	if (!condition)
	{
		std::cout << message << std::endl;
		throw std::runtime_error(message);
	}
}

std::optional<size_t>
get_glsl_type_size_in_bytes(const std::string & type)
{
	if (type == "float") return sizeof(float);
	if (type == "int") return sizeof(int);
	if (type == "mat2") return sizeof(mat2);
	if (type == "mat3") return sizeof(mat3);
	if (type == "mat4") return sizeof(mat4);
	if (type == "vec2") return sizeof(vec2);
	if (type == "vec3") return sizeof(vec3);
	if (type == "vec4") return sizeof(vec4);
	return std::nullopt;
}

uint32_t
find_mem_type(const vk::PhysicalDevice & physical_device,
			  const uint32_t type_filter,
			  const vk::MemoryPropertyFlags& mem_property_flags)
{
	vk::PhysicalDeviceMemoryProperties mem_properties = physical_device.getMemoryProperties();
	for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++)
		if (type_filter & (1 << i))
			if (mem_properties.memoryTypes[i].propertyFlags & mem_property_flags)
				return i;
	return -1;
}

template <typename T>
T
div_and_ceil(const T & numerator,
			 const T & denominator)
{
	static_assert(std::is_integral<T>::value,
				  "div_and_ceil supports only integral types (int, uint, bool, long and their variants)");
	return (numerator + denominator - 1) / denominator;
}

template <typename T>
std::ostream & operator<<(std::ostream & os, const std::vector<T> & v)
{
	os << "< ";
	for (size_t i = 0; i < v.size(); i++)
	{
		if (i <= 3 || i > v.size() - 3)
		{
			// print value
			if (i == 3)
			{
				os << "...";
			}
			else
			{
				if constexpr (std::is_same<T, std::string>::value)
				{
					os << "\"" << v[i] << "\"";
				}
				else
				{
					os << v[i];
				}
			}

			// print colon
			if (i != v.size() - 1)
			{
				os << ", ";
			}
		}
	}
	os << "> ";
	return os;
}

std::string
load_file(const std::filesystem::path & path)
{
	std::ifstream ifs;
	ifs.open(path);
	THROW_ASSERT(ifs.is_open(), "cannot open file :" + std::filesystem::absolute(path).string());
	std::stringstream buffer;
	buffer << ifs.rdbuf();
	return buffer.str();
}

std::string
remove_chars(const std::string & str,
					 const std::string & special_chars)
{
	std::string result = str;
	for (size_t i_char = 0; i_char < special_chars.size(); i_char++)
	{
		result.erase(std::remove(result.begin(), result.end(), special_chars[i_char]), result.end());
	}
	return result;
}

std::string
remove_line_comments(const std::string & str,
					 const std::vector<std::string> & comment_strs)
{
	std::string result = "";
	size_t prev_endline_pos = 0;
	while (true)
	{
		size_t comment_pos = std::string::npos;
		size_t comment_index = 0;
		for (size_t i_comment = 0; i_comment < comment_strs.size(); i_comment++)
		{
			const size_t comment_i_pos = str.find(comment_strs[i_comment], prev_endline_pos);
			comment_pos = std::min(comment_pos, comment_i_pos);
		}
		if (comment_pos == std::string::npos)
		{
			break;
		}
		size_t endline_pos = str.find("\n", comment_pos);
		if (endline_pos == std::string::npos)
		{
			endline_pos = str.size();
		}
		result += str.substr(prev_endline_pos, comment_pos - prev_endline_pos);
		prev_endline_pos = endline_pos + 1;
	}
	result += str.substr(prev_endline_pos, str.size());
	return result;
}

std::string
remove_block_comments(const std::string & str,
					  const std::vector<std::pair<std::string, std::string>> & comment_strs)
{
	std::string result = "";
	size_t prev_endline_pos = 0;
	while (true)
	{
		size_t comment_begin_pos = std::string::npos;
		size_t comment_index = 0;
		for (size_t i_comment = 0; i_comment < comment_strs.size(); i_comment++)
		{
			const size_t comment_begin_i_pos = str.find(comment_strs[i_comment].first, prev_endline_pos);
			if (comment_begin_i_pos < comment_begin_pos)
			{
				comment_begin_pos = comment_begin_i_pos;
				comment_index = i_comment;
			}
		}
		if (comment_begin_pos == std::string::npos)
		{
			break;
		}
		const std::string & comment_end = comment_strs[comment_index].second;
		size_t comment_end_pos = str.find(comment_end, comment_begin_pos);
		THROW_ASSERT(comment_end_pos != std::string::npos, "could not find closing comment token :" + comment_end);
		result += str.substr(prev_endline_pos, comment_begin_pos - prev_endline_pos);
		prev_endline_pos = comment_end_pos + comment_end.size();
	}
	result += str.substr(prev_endline_pos, str.size());
	return result;
}

std::vector<std::string>
tokenize(const std::string & str,
		 const std::string & delims,
		 const bool keep_delims)
{
	size_t prev_split = 0;
	std::vector<std::string> result;
	for (size_t i_char = 0; i_char < str.size(); i_char++)
	{
		// check if current character is a delimeter or not
		bool is_char_delim = false;
		for (const char & delim : delims)
		{
			if (delim == str[i_char])
			{
				is_char_delim = true;
				break;
			}
		}

		// check if current character is a whitespace or not
		bool is_char_whitespace = std::isspace(str[i_char]);

		// if yes, push new string into result and update the prev_split.
		if (is_char_delim || is_char_whitespace)
		{
			size_t split = i_char;
			if (split - prev_split > 0)
			{
				result.push_back(str.substr(prev_split, split - prev_split));
			}
			if (is_char_delim && keep_delims)
			{
				const std::string delim = str.substr(split, 1);
				result.push_back(delim);
			}
			prev_split = split + 1;
		}
	}

	if (str.size() - prev_split > 0)
	{
		result.push_back(str.substr(prev_split));
	}

	return result;
}

template <typename T>
constexpr
const T
digit(const T num, const uint32_t i_digit)
{
	T p0 = 1;
	for (size_t i = 0; i < i_digit; i++) p0 *= T(10);
	const T p1 = p0 * T(10);
	const T front_value_back = num;
	const T value_back = num % p1;
	const T value = num % p0;
	return value;
}

template <typename T>
std::vector<T *>
raw_ptrs(const std::vector<std::unique_ptr<T>> & ptrs,
		 size_t begin = 0,
		 size_t end = -1)
{
	end = end == -1 ? ptrs.size() : end;
	std::vector<T *> result(end - begin);
	for (size_t i = begin; i < end; i++)
	{
		result[i] = ptrs[i].get();
	}
	return result;
}

template <typename T, typename U>
std::vector<const T *>
raw_ptrs(const std::vector<vk::UniqueHandle<T, U>> & ptrs,
		 size_t begin = 0,
		 size_t end = -1)
{
	end = end == -1 ? ptrs.size() : end;
	std::vector<const T *> result(end - begin);
	for (size_t i = begin; i < end; i++)
	{
		result[i] = &(ptrs[i].get());
	}
	return result;
}


// See https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/chap33.html#acceleration-structure
struct VkGeometryInstanceNV
{
	/// Transform matrix, containing only the top 3 rows
	float transform[12];
	/// Instance index
	uint32_t instanceId : 24;
	/// Visibility mask
	uint32_t mask : 8;
	/// Index of the hit group which will be invoked when a ray hits the instance
	uint32_t hitGroupId : 24;
	/// Instance flags, such as culling
	uint32_t flags : 8;
	/// Opaque handle of the bottom-level acceleration structure
	uint64_t accelerationStructureHandle;
};
