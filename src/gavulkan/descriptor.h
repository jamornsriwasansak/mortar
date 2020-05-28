#pragma once

#include <vulkan/vulkan.hpp>

#include "gavulkan/image.h"
#include "gavulkan/buffer.h"

struct DescriptorSetLayoutInfo
{
	vk::UniqueDescriptorSetLayout			m_vk_descriptor_set_layout;
	size_t									m_num_bindings = -1;
	size_t									m_set = -1;
};

struct DescriptorSetsBuilder
{
	size_t																			m_set = -1;

	std::vector<vk::DescriptorSet>													m_vk_descriptor_sets;
	size_t																			m_num_swapchain_frames;

	std::vector<std::unique_ptr<vk::DescriptorBufferInfo>>							m_descriptor_buffer_infos;
	std::vector<std::unique_ptr<vk::DescriptorBufferInfo[]>>						m_descriptor_buffer_info_arrays;
	std::vector<std::unique_ptr<vk::DescriptorImageInfo>>							m_descriptor_image_infos;
	std::vector<std::unique_ptr<vk::DescriptorImageInfo[]>>							m_descriptor_image_info_arrays;
	std::vector<std::unique_ptr<vk::WriteDescriptorSetAccelerationStructureKHR>>	m_write_descriptor_set_accel_struct;
	std::vector<vk::WriteDescriptorSet>												m_write_descriptor_sets;

	DescriptorSetsBuilder(const size_t set,
						  const DescriptorSetLayoutInfo * descriptor_set_layout_info,
						  const std::vector<vk::DescriptorSet> & vk_descriptor_sets,
						  const size_t num_swapchain_frames):
		m_set(set),
		m_vk_descriptor_sets(vk_descriptor_sets),
		m_num_swapchain_frames(num_swapchain_frames)
	{
	}

	DescriptorSetsBuilder &
	set_uniform_buffer(const size_t i_frame,
					   const size_t i_binding,
					   const Buffer & buffer)
	{
		/*
		// grab uniform info for this binding
		const ShaderUniformInfo * shader_uniform = m_descriptor_set_layout_info->m_shader_uniforms[i_binding];

		// make sure everything makes sense
		THROW_ASSERT(shader_uniform->does_use_buffer(),
					 "buffer is binded to desctiptor image");
		THROW_ASSERT(shader_uniform->m_size_in_bytes == buffer.m_element_size_in_bytes * buffer.m_num_elements,
					 "size of uniform specified in shader mismatches size of uniform provided by a buffer");
		THROW_ASSERT(shader_uniform->m_binding == i_binding,
					 "binding number mismatch. this is probably my bug");
		*/

		// all necessary info about buffer
		std::unique_ptr<vk::DescriptorBufferInfo> descriptor_buffer_info = std::make_unique<vk::DescriptorBufferInfo>();
		{
			descriptor_buffer_info->setBuffer(*buffer.m_vk_buffer);
			descriptor_buffer_info->setOffset(0);
			descriptor_buffer_info->setRange(buffer.size_in_bytes());
		}

		int num_descriptors = 1;

		// WriteDescriptorSet for buffer
		vk::WriteDescriptorSet write_descriptor_set = {};
		{
			write_descriptor_set.setDstSet(m_vk_descriptor_sets[i_frame]);
			write_descriptor_set.setDstBinding(static_cast<uint32_t>(i_binding));
			write_descriptor_set.setDstArrayElement(0);
			write_descriptor_set.setDescriptorCount(static_cast<uint32_t>(num_descriptors));
			write_descriptor_set.setDescriptorType(vk::DescriptorType::eUniformBuffer);
			write_descriptor_set.setPBufferInfo(descriptor_buffer_info.get());
			write_descriptor_set.setPImageInfo(nullptr);
		}

		// push back both
		m_descriptor_buffer_infos.push_back(std::move(descriptor_buffer_info));
		m_write_descriptor_sets.push_back(write_descriptor_set);

		return *this;
	}
	DescriptorSetsBuilder &
	set_uniform_buffer(const size_t i_binding,
					   const Buffer & buffer)
	{
		for (size_t i_frame = 0; i_frame < m_num_swapchain_frames; i_frame++)
		{
			set_uniform_buffer(i_frame,
							   i_binding,
							   buffer);
		}
		return *this;
	}

	DescriptorSetsBuilder &
	set_uniform_buffer_chain(const size_t i_binding,
							 const std::vector<Buffer> & buffers)
	{
		for (size_t i_frame = 0; i_frame < m_num_swapchain_frames; i_frame++)
		{
			set_uniform_buffer(i_frame,
							   i_binding,
							   buffers[i_frame]);
		}

		return *this;
	}

	DescriptorSetsBuilder &
	set_storage_buffer(const size_t i_frame,
					   const size_t i_binding,
					   const Buffer & buffer)
	{
		// all necessary info about buffer
		std::unique_ptr<vk::DescriptorBufferInfo> descriptor_buffer_info = std::make_unique<vk::DescriptorBufferInfo>();
		{
			descriptor_buffer_info->setBuffer(*buffer.m_vk_buffer);
			descriptor_buffer_info->setOffset(0);
			descriptor_buffer_info->setRange(buffer.size_in_bytes());
		}

		// WriteDescriptorSet for buffer
		vk::WriteDescriptorSet write_descriptor_set = {};
		{
			write_descriptor_set.setDstSet(m_vk_descriptor_sets[i_frame]);
			write_descriptor_set.setDstBinding(static_cast<uint32_t>(i_binding));
			write_descriptor_set.setDstArrayElement(0);
			write_descriptor_set.setDescriptorCount(1);
			write_descriptor_set.setDescriptorType(vk::DescriptorType::eStorageBuffer);
			write_descriptor_set.setPBufferInfo(descriptor_buffer_info.get());
			write_descriptor_set.setPImageInfo(nullptr);
		}

		// push back both
		m_descriptor_buffer_infos.push_back(std::move(descriptor_buffer_info));
		m_write_descriptor_sets.push_back(write_descriptor_set);

		return *this;
	}

	DescriptorSetsBuilder &
	set_storage_buffer(const size_t i_binding,
					   const Buffer & buffer)
	{
		for (size_t i = 0; i < m_num_swapchain_frames; i++)
		{
			set_storage_buffer(i,
							   i_binding,
							   buffer);
		}

		return *this;
	}

	DescriptorSetsBuilder &
	set_storage_buffers_array(const size_t i_frame,
							  const size_t i_binding,
							  const std::vector<const Buffer *> & buffers)
	{
		// all necessary info about buffer
		std::unique_ptr<vk::DescriptorBufferInfo[]> descriptor_buffer_infos = std::make_unique<vk::DescriptorBufferInfo[]>(buffers.size());
		for (size_t i_buffer = 0; i_buffer < buffers.size(); i_buffer++)
		{
			descriptor_buffer_infos[i_buffer].setBuffer(*buffers[i_buffer]->m_vk_buffer);
			descriptor_buffer_infos[i_buffer].setOffset(0);
			descriptor_buffer_infos[i_buffer].setRange(buffers[i_buffer]->size_in_bytes());
		}

		// WriteDescriptorSet for buffer
		vk::WriteDescriptorSet write_descriptor_set = {};
		{
			write_descriptor_set.setDstSet(m_vk_descriptor_sets[i_frame]);
			write_descriptor_set.setDstBinding(static_cast<uint32_t>(i_binding));
			write_descriptor_set.setDstArrayElement(0);
			write_descriptor_set.setDescriptorCount(static_cast<uint32_t>(buffers.size()));
			write_descriptor_set.setDescriptorType(vk::DescriptorType::eStorageBuffer);
			write_descriptor_set.setPBufferInfo(descriptor_buffer_infos.get());
			write_descriptor_set.setPImageInfo(nullptr);
		}

		// push back both
		m_descriptor_buffer_info_arrays.push_back(std::move(descriptor_buffer_infos));
		m_write_descriptor_sets.push_back(write_descriptor_set);

		return *this;
	}

	DescriptorSetsBuilder &
	set_storage_buffers_array(const size_t i_binding,
							  const std::vector<const Buffer *> & buffers)
	{
		for (size_t i_frame = 0; i_frame < m_num_swapchain_frames; i_frame++)
		{
			set_storage_buffers_array(i_frame,
									  i_binding,
									  buffers);
		}

		return *this;
	}

	template <typename T>
	DescriptorSetsBuilder &
	set_sampler(const size_t i_binding,
				const RgbaImage<T> & image)
	{
		/*
		// grab uniform info for this binding
		const ShaderUniformInfo * shader_uniform = m_descriptor_set_layout_info->m_shader_uniforms[i_binding];

		// make sure everything makes sense
		THROW_ASSERT(!shader_uniform->does_use_buffer(),
					 "image is binded to descriptor buffer");
		THROW_ASSERT(shader_uniform->m_binding == i_binding,
					 "binding number mismatch. this is probably my bug");
		*/

		// describe what the image is like
		std::unique_ptr<vk::DescriptorImageInfo> descriptor_image_info = std::make_unique<vk::DescriptorImageInfo>();
		{
			descriptor_image_info->setImageLayout(image.m_vk_image_layout);
			descriptor_image_info->setImageView(*image.m_vk_image_view);
			descriptor_image_info->setSampler(*image.m_vk_sampler);
		}

		for (size_t i_frame = 0; i_frame < m_num_swapchain_frames; i_frame++)
		{
			// WriteDescriptorSet for image
			vk::WriteDescriptorSet write_descriptor_set = {};
			{
				write_descriptor_set.setDstSet(m_vk_descriptor_sets[i_frame]);
				write_descriptor_set.setDstBinding(static_cast<uint32_t>(i_binding));
				write_descriptor_set.setDstArrayElement(0);
				write_descriptor_set.setDescriptorCount(1);
				write_descriptor_set.setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
				write_descriptor_set.setPBufferInfo(nullptr);
				write_descriptor_set.setPImageInfo(descriptor_image_info.get());
			}
			m_write_descriptor_sets.push_back(write_descriptor_set);
		}
		// push back descriptor_image
		m_descriptor_image_infos.push_back(std::move(descriptor_image_info));

		return *this;
	}

	template <typename T>
	DescriptorSetsBuilder &
	set_samplers(const size_t i_binding,
				 const std::vector<const RgbaImage<T> *> & images)
	{
		THROW_ASSERT(images.size() > 0,
					 "number of sampler must be greater zero");
		std::unique_ptr<vk::DescriptorImageInfo[]> descriptor_image_infos = std::make_unique<vk::DescriptorImageInfo[]>(images.size());
		for (size_t i_image = 0; i_image < images.size(); i_image++)
		{
			// describe what the image is like
			{
				descriptor_image_infos[i_image].setImageLayout(images[i_image]->m_vk_image_layout);
				descriptor_image_infos[i_image].setImageView(*images[i_image]->m_vk_image_view);
				descriptor_image_infos[i_image].setSampler(*images[i_image]->m_vk_sampler);
			}
		}

		for (size_t i_frame = 0; i_frame < m_num_swapchain_frames; i_frame++)
		{
			// WriteDescriptorSet for image
			vk::WriteDescriptorSet write_descriptor_set = {};
			{
				write_descriptor_set.setDstSet(m_vk_descriptor_sets[i_frame]);
				write_descriptor_set.setDstBinding(static_cast<uint32_t>(i_binding));
				write_descriptor_set.setDstArrayElement(0);
				write_descriptor_set.setDescriptorCount(static_cast<uint32_t>(images.size()));
				write_descriptor_set.setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
				write_descriptor_set.setPBufferInfo(nullptr);
				write_descriptor_set.setPImageInfo(descriptor_image_infos.get());
			}
			m_write_descriptor_sets.push_back(write_descriptor_set);
		}
		// push back descriptor_image
		m_descriptor_image_info_arrays.push_back(std::move(descriptor_image_infos));

		return *this;
	}


	template <typename T>
	DescriptorSetsBuilder &
	set_storage_image(const size_t i_frame,
					  const size_t i_binding,
					  const RgbaImage<T> & image)
	{
		// describe what the image is like
		std::unique_ptr<vk::DescriptorImageInfo> descriptor_image_info = std::make_unique<vk::DescriptorImageInfo>();
		{
			descriptor_image_info->setImageLayout(vk::ImageLayout::eGeneral);
			descriptor_image_info->setImageView(*image.m_vk_image_view);
			descriptor_image_info->setSampler(nullptr);
		}

		// WriteDescriptorSet for image
		vk::WriteDescriptorSet write_descriptor_set = {};
		{
			write_descriptor_set.setDstSet(m_vk_descriptor_sets[i_frame]);
			write_descriptor_set.setDstBinding(static_cast<uint32_t>(i_binding));
			write_descriptor_set.setDstArrayElement(0);
			write_descriptor_set.setDescriptorCount(1);
			write_descriptor_set.setDescriptorType(vk::DescriptorType::eStorageImage);
			write_descriptor_set.setPBufferInfo(nullptr);
			write_descriptor_set.setPImageInfo(descriptor_image_info.get());
		}
		m_write_descriptor_sets.push_back(write_descriptor_set);
		m_descriptor_image_infos.push_back(std::move(descriptor_image_info));
		return *this;
	}

	template <typename T>
	DescriptorSetsBuilder &
	set_storage_image(const size_t i_binding,
					  const RgbaImage<T> & image)
	{
		for (size_t i_frame = 0; i_frame < m_num_swapchain_frames; i_frame++)
		{
			set_storage_image(i_frame,
							  i_binding,
							  image);
		}
		return *this;
	}

	DescriptorSetsBuilder &
	set_accel_struct(const size_t i_binding,
					 const vk::AccelerationStructureKHR & accel)
	{
		auto write_descriptor_set_accel = std::make_unique<vk::WriteDescriptorSetAccelerationStructureKHR>();
		{
			write_descriptor_set_accel->setAccelerationStructureCount(1);
			write_descriptor_set_accel->setPAccelerationStructures(&accel);
		}

		for (size_t i_frame = 0; i_frame < m_num_swapchain_frames; i_frame++)
		{
			vk::WriteDescriptorSet write_descriptor_set = {};
			{
				write_descriptor_set.setDstSet(m_vk_descriptor_sets[i_frame]);
				write_descriptor_set.setDstBinding(static_cast<uint32_t>(i_binding));
				write_descriptor_set.setDstArrayElement(0);
				write_descriptor_set.setDescriptorCount(1);
				write_descriptor_set.setDescriptorType(vk::DescriptorType::eAccelerationStructureKHR);
				write_descriptor_set.setPBufferInfo(nullptr);
				write_descriptor_set.setPImageInfo(nullptr);
				write_descriptor_set.setPNext(write_descriptor_set_accel.get());
			}
			m_write_descriptor_sets.push_back(write_descriptor_set);
		}
		m_write_descriptor_set_accel_struct.push_back(std::move(write_descriptor_set_accel));

		return *this;
	}

	std::vector<vk::DescriptorSet>
	build()
	{
		Core::Inst().m_vk_device->updateDescriptorSets(m_write_descriptor_sets,
													   nullptr);
		m_descriptor_buffer_infos.clear();
		m_descriptor_image_infos.clear();
		m_descriptor_buffer_info_arrays.clear();
		m_descriptor_image_info_arrays.clear();
		m_write_descriptor_sets.clear();

		return std::move(m_vk_descriptor_sets);
	}
};

struct DescriptorManager
{
	std::vector<DescriptorSetLayoutInfo>	m_descriptor_set_layout_infos;
	std::optional<vk::PushConstantRange>	m_vk_push_constant_range;

	DescriptorManager(const std::vector<const Shader *> & shaders)
	{
		const size_t num_descriptor_sets = count_num_descriptor_sets(shaders);

		// create layout for all possible sets
		for (size_t i_set = 0; i_set < num_descriptor_sets; i_set++)
		{
			DescriptorSetLayoutInfo descriptor_set_layout_info;

			// search which binding correspond to i_set
			std::map<size_t, vk::DescriptorSetLayoutBinding> binding_to_descriptor_layout_bindings;
			for (const Shader * shader : shaders)
			{
				for (const ShaderUniformInfo & shader_uniform : shader->m_uniforms)
				{
					if (shader_uniform.m_is_push_constant)
					{
						// check whether push_constant exists or not
						if (m_vk_push_constant_range.has_value())
						{
							THROW_ASSERT(m_vk_push_constant_range->size == shader_uniform.m_size_in_bytes,
										 "size in bytes of push_constants mismatch between different shader stage.");
							const vk::ShaderStageFlags old_stage_flags = m_vk_push_constant_range->stageFlags;
							const vk::ShaderStageFlags new_stage_flags = old_stage_flags | shader->m_vk_shader_stage;
							m_vk_push_constant_range->setStageFlags(new_stage_flags);
						}
						else
						{
							vk::PushConstantRange push_constant_range;
							push_constant_range.setSize(shader_uniform.m_size_in_bytes);
							push_constant_range.setStageFlags(shader->m_vk_shader_stage);
							m_vk_push_constant_range = push_constant_range;
						}
					}
					else if (shader_uniform.m_set == i_set)
					{
						// check whether binding is clashing or not
						auto itor_descriptor_layout_binding = binding_to_descriptor_layout_bindings.find(shader_uniform.m_binding);
						if (itor_descriptor_layout_binding == binding_to_descriptor_layout_bindings.end())
						{
							// create new binding
							vk::DescriptorSetLayoutBinding descriptor_layout_binding;
							descriptor_layout_binding.setBinding(shader_uniform.m_binding);
							descriptor_layout_binding.setDescriptorType(shader_uniform.get_descriptor_type());
							descriptor_layout_binding.setDescriptorCount(shader_uniform.m_num_descriptors);
							descriptor_layout_binding.setStageFlags(shader->m_vk_shader_stage);
							descriptor_layout_binding.setPImmutableSamplers(nullptr);

							binding_to_descriptor_layout_bindings.insert({ shader_uniform.m_binding, descriptor_layout_binding });
						}
						else
						{
							// edit the existing binding
							vk::DescriptorSetLayoutBinding & descriptor_layout_binding = itor_descriptor_layout_binding->second;

							// make sure that stage flag is not set (e.g. given a set, same binding cannot appear same shader stage twice)
							const vk::ShaderStageFlags old_stage_flags = descriptor_layout_binding.stageFlags;
							const vk::ShaderStageFlags new_stage_flags = old_stage_flags | shader->m_vk_shader_stage;
							THROW_ASSERT(old_stage_flags != new_stage_flags,
										 "same binding of a set appears in shader with same stage.");

							// update shader stage
							descriptor_layout_binding.setStageFlags(new_stage_flags);

							// check whether the binding matches or not
							THROW_ASSERT(descriptor_layout_binding.descriptorType == shader_uniform.get_descriptor_type(),
										 "descriptor type mismatch between different type of shader");
						}
					}
				}
			}

			// check whether uniform indices match binding
			for (size_t i_binding = 0; i_binding < binding_to_descriptor_layout_bindings.size(); i_binding++)
			{
				THROW_ASSERT(binding_to_descriptor_layout_bindings[i_binding].binding == static_cast<uint32_t>(i_binding),
							 "binding number \"" + std::to_string(i_binding) + "\" missing");
			}

			// turn map into vector
			std::vector<vk::DescriptorSetLayoutBinding> descriptor_layout_bindings;
			descriptor_layout_bindings.reserve(binding_to_descriptor_layout_bindings.size());
			for (size_t i_binding = 0; i_binding < binding_to_descriptor_layout_bindings.size(); i_binding++)
			{
				descriptor_layout_bindings.push_back(binding_to_descriptor_layout_bindings[i_binding]);
			}

			// create a set of all descriptor for this pipeline
			vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_ci = {};
			{
				descriptor_set_layout_ci.setBindingCount(static_cast<uint32_t>(descriptor_layout_bindings.size()));
				descriptor_set_layout_ci.setPBindings(descriptor_layout_bindings.data());
			}

			// setup set layout
			descriptor_set_layout_info.m_vk_descriptor_set_layout = Core::Inst().m_vk_device->createDescriptorSetLayoutUnique(descriptor_set_layout_ci);
			descriptor_set_layout_info.m_num_bindings = binding_to_descriptor_layout_bindings.size();
			descriptor_set_layout_info.m_set = i_set;

			// push into vector
			m_descriptor_set_layout_infos.push_back(std::move(descriptor_set_layout_info));
		}
	}

	size_t
	count_num_descriptor_sets(const std::vector<const Shader *> & shaders) const
	{
		size_t num_description_set = 0;
		for (const Shader * shader : shaders)
		{
			for (const ShaderUniformInfo& shader_uniform : shader->m_uniforms)
			{
				num_description_set = std::max(num_description_set, size_t(shader_uniform.m_set) + 1);
			}
		}
		return num_description_set;
	}

	std::vector<vk::DescriptorSetLayout>
	get_vk_descriptor_set_layouts() const
	{
		std::vector<vk::DescriptorSetLayout> result(m_descriptor_set_layout_infos.size());
		for (size_t i = 0; i < m_descriptor_set_layout_infos.size(); i++)
		{
			result[i] = *m_descriptor_set_layout_infos[i].m_vk_descriptor_set_layout;
		}
		return result;
	}

	DescriptorSetsBuilder
	make_descriptor_sets_builder(const size_t i_set,
								 const size_t num_swapchain_images = 1) const
	{
		// create descriptor set for the length of swapchain
		std::vector<vk::DescriptorSetLayout> descriptor_set_layouts(num_swapchain_images,
																	*m_descriptor_set_layout_infos[i_set].m_vk_descriptor_set_layout);
		vk::DescriptorSetAllocateInfo descriptor_set_alloc_info = {};
		{
			descriptor_set_alloc_info.setDescriptorPool(*Core::Inst().m_vk_descriptor_pool);
			descriptor_set_alloc_info.setDescriptorSetCount(static_cast<uint32_t>(descriptor_set_layouts.size()));
			descriptor_set_alloc_info.setPSetLayouts(descriptor_set_layouts.data());
		}
		std::vector<vk::DescriptorSet> descriptor_sets = Core::Inst().m_vk_device->allocateDescriptorSets(descriptor_set_alloc_info);
		return DescriptorSetsBuilder(i_set,
									 &m_descriptor_set_layout_infos[i_set],
									 descriptor_sets,
									 num_swapchain_images);
	}
};
