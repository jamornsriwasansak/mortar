#pragma once

#include "common/mortar.h"

#include "gavulkan/appcore.h"
#include "gavulkan/buffer.h"
#include "gavulkan/image.h"

struct Envmap
{
	RgbaImage2d<float>	m_image;
	uvec2				m_resolution;
	float				m_emissive_weight = 0.0f;
	Buffer				m_cdf_table;

	Envmap(const bool build_cdf_table = false)
	{
		// init emissive weight with zero
		m_emissive_weight = 0.0f;

		// init envmap with a blank image
		std::vector<vec4> blank_image(1, vec4(0.0f));
		m_image = RgbaImage2d<float>(blank_image.data(),
									 1, // width = 1
									 1, // height = 1
									 vk::ImageTiling::eOptimal,
									 RgbaImage2d<float>::RgbaUsageFlagBits);
		m_image.init_sampler(true);
		m_resolution = uvec2(1, 1);

		// build cdf table if requested
		if (build_cdf_table)
		{
			create_cdf_table(blank_image.data(),
							 1,
							 1);
		}
	}
	
	Envmap(const std::filesystem::path & filepath,
		   const bool build_cdf_table = false)
	{
		StbiFloat32Result stbi_float_result(filepath);

		// compute emissive weight by averaging all the pixels
		vec3 sum = vec3(0.0f);
		const vec4 * tdata = reinterpret_cast<const vec4 *>(stbi_float_result.m_stbi_data);
		const int width = stbi_float_result.m_width;
		const int height = stbi_float_result.m_height;
		for (int r = 0; r < height; r++)
		{
			// compute cosine weighted value
			const float v = static_cast<float>(r) / static_cast<float>(height);
			const float sine_term = std::sin(pi<float>() * v);

			for (int c = 0; c < width; c++)
			{
				// get pixel value
				const int tdata_index = (r * width + c);
				const vec3 value(tdata[tdata_index]);

				// add cosine weighted pixel value to sum
				sum += sine_term * value;
			}
		}
		const vec3 average = sum / static_cast<float>(width * height);

		m_emissive_weight = length(average);
		m_image = RgbaImage2d<float>(stbi_float_result);
		m_image.init_sampler(true);
		m_resolution = uvec2(width,
							 height);

		// build cdf table if requested
		if (build_cdf_table)
		{
			create_cdf_table(tdata,
							 width,
							 height);
		}
	}

	void
	create_cdf_table(const vec4 * data,
					 const int width,
					 const int height)
	{
		const int num_pixels = width * height;
		std::vector<float> pdf_table(num_pixels);
		std::vector<float> cdf_table(num_pixels + 1);

		// compute the unnormalized probability of sampling a pixel
		for (int r = 0; r < height; r++)
		{
			// compute cosine weighted value
			const float v = static_cast<float>(r) / static_cast<float>(height);
			const float sine_term = std::sin(pi<float>() * v);

			for (int c = 0; c < width; c++)
			{
				// get pixel value
				const int data_index = (r * width + c);
				const vec3 value(data[data_index]);
				const vec3 weighted_value = sine_term * value;

				// assign value to pdf_table
				pdf_table[data_index] = length(value);
			}
		}

		// compute unnormalized cdf_table
		cdf_table[0] = 0.0f;
		for (int i = 0; i < num_pixels; i++)
		{
			cdf_table[i + 1] = pdf_table[i] + cdf_table[i];
		}

		// normalize both tables
		float sum = 0.0f;
		for (int i = 0; i < num_pixels; i++)
		{
			sum += pdf_table[i];
		}
		sum = (sum < SMALL_VALUE) ? 1.0f : sum; // prevent sum = 0.0f
		for (int i = 0; i < num_pixels + 1; i++)
		{
			cdf_table[i] /= sum;
		}

		// create cdf table
		m_cdf_table = Buffer(cdf_table,
							 vk::MemoryPropertyFlagBits::eDeviceLocal,
							 vk::BufferUsageFlagBits::eStorageBuffer);
	}
};