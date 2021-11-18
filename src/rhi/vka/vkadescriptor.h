#pragma once

#include "vkacommon.h"

#ifdef USE_VKA

#include "../shadercompiler/hlsldxccompiler.h"
#include "vkabuffer.h"
#include "vkadescriptorpool.h"
#include "vkadevice.h"
#include "vkarasterpipeline.h"
#include "vkaraytracingaccel.h"
#include "vkaraytracingpipeline.h"
#include "vkasampler.h"
#include "vkatexture.h"

namespace VKA_NAME
{
struct DescriptorSet
{
    vk::DescriptorSet m_vk_descriptor_set;
    const Device * m_device;
    vk::PipelineLayout m_vk_pipeline_layout;

    // TODO:: avoid std::list
    std::list<vk::DescriptorBufferInfo> m_descriptor_buffer_infos;
    std::list<vk::DescriptorImageInfo> m_descriptor_image_infos;
    std::list<vk::WriteDescriptorSetAccelerationStructureKHR> m_descriptor_accel_infos;

    std::vector<vk::WriteDescriptorSet> m_write_descriptor_set;

    DescriptorSet() {}

    DescriptorSet(const Device * device,
                  const RasterPipeline & pipeline,
                  DescriptorPool * descriptor_pool,
                  const size_t i_set,
                  const std::string & name = "")
    : DescriptorSet(device,
                    pipeline.m_vk_pipeline_layout.get(),
                    pipeline.m_vk_descriptor_set_layouts[i_set].get(),
                    descriptor_pool->m_vk_descriptor_pool.get(),
                    name)
    {
    }

    DescriptorSet(const Device * device,
                  const RayTracingPipeline & pipeline,
                  DescriptorPool * descriptor_pool,
                  const size_t i_set,
                  const std::string & name = "")
    : DescriptorSet(device,
                    pipeline.m_vk_pipeline_layout.get(),
                    pipeline.m_vk_descriptor_set_layouts[i_set].get(),
                    descriptor_pool->m_vk_descriptor_pool.get(),
                    name)
    {
    }

    DescriptorSet(const Device * device,
                  const vk::PipelineLayout vk_pipeline_layout,
                  const vk::DescriptorSetLayout vk_desc_set,
                  const vk::DescriptorPool vk_desc_pool,
                  const std::string & name = "")
    : m_device(device), m_vk_pipeline_layout(vk_pipeline_layout)
    {
        vk::DescriptorSetAllocateInfo set_ai = {};
        set_ai.setDescriptorPool(vk_desc_pool);
        set_ai.setDescriptorSetCount(1);
        set_ai.setPSetLayouts(&vk_desc_set);
        m_vk_descriptor_set = std::move(device->m_vk_ldevice->allocateDescriptorSets(set_ai)[0]);
        device->name_vkhpp_object<vk::DescriptorSet, vk::DescriptorSet::CType>(m_vk_descriptor_set, name);
    }


    DescriptorSet &
    set_t_ray_tracing_accel(const size_t binding, const RayTracingTlas & tlas, const size_t i_element = 0)
    {
        vk::WriteDescriptorSetAccelerationStructureKHR write_descriptor = {};
        write_descriptor.setAccelerationStructureCount(1);
        write_descriptor.setPAccelerationStructures(&tlas.m_vk_accel_struct.get());
        m_descriptor_accel_infos.push_back(write_descriptor);

        vk::WriteDescriptorSet write_descriptor_set = {};
        write_descriptor_set.setDstSet(m_vk_descriptor_set);
        write_descriptor_set.setDstBinding(static_cast<uint32_t>(binding + HlslDxcCompiler::TShift));
        write_descriptor_set.setDstArrayElement(static_cast<uint32_t>(i_element));
        write_descriptor_set.setDescriptorCount(1);
        write_descriptor_set.setDescriptorType(vk::DescriptorType::eAccelerationStructureKHR);
        write_descriptor_set.setPBufferInfo(nullptr);
        write_descriptor_set.setPImageInfo(nullptr);
        write_descriptor_set.setPNext(&m_descriptor_accel_infos.back());
        m_write_descriptor_set.push_back(write_descriptor_set);

        return *this;
    }

    DescriptorSet &
    set_b_constant_buffer(const size_t binding, const Buffer & buffer, const size_t i_element = 0)
    {
        vk::DescriptorBufferInfo buf_info = {};
        buf_info.setBuffer(buffer.m_vma_buffer_bundle->m_vk_buffer);
        buf_info.setOffset(0);
        buf_info.setRange(buffer.m_size_in_bytes);
        m_descriptor_buffer_infos.push_back(buf_info);

        vk::WriteDescriptorSet write_descriptor = {};
        write_descriptor.setDstSet(m_vk_descriptor_set);
        write_descriptor.setDstBinding(static_cast<uint32_t>(binding + HlslDxcCompiler::BShift));
        write_descriptor.setDstArrayElement(static_cast<uint32_t>(i_element));
        write_descriptor.setDescriptorCount(1);
        write_descriptor.setDescriptorType(vk::DescriptorType::eUniformBuffer);
        write_descriptor.setPBufferInfo(&m_descriptor_buffer_infos.back());
        m_write_descriptor_set.push_back(write_descriptor);
        return *this;
    }

    DescriptorSet &
    set_t_structured_buffer(const size_t binding,
                            const Buffer & buffer,
                            const size_t stride,
                            const size_t num_elements,
                            const size_t i_element     = 0,
                            const size_t first_element = 0)
    {
        vk::DescriptorBufferInfo buf_info = {};
        buf_info.setBuffer(buffer.m_vma_buffer_bundle->m_vk_buffer);
        buf_info.setOffset(stride * first_element);
        buf_info.setRange(stride * num_elements);
        m_descriptor_buffer_infos.push_back(buf_info);

        vk::WriteDescriptorSet write_descriptor = {};
        write_descriptor.setDstSet(m_vk_descriptor_set);
        write_descriptor.setDstBinding(static_cast<uint32_t>(binding + HlslDxcCompiler::TShift));
        write_descriptor.setDstArrayElement(static_cast<uint32_t>(i_element));
        write_descriptor.setDescriptorCount(1);
        write_descriptor.setDescriptorType(vk::DescriptorType::eStorageBuffer);
        write_descriptor.setPBufferInfo(&m_descriptor_buffer_infos.back());
        m_write_descriptor_set.push_back(write_descriptor);
        return *this;
    }

    DescriptorSet &
    set_t_byte_address_buffer(const size_t binding,
                              const Buffer & buffer,
                              const size_t stride,
                              const size_t num_elements,
                              const size_t i_element     = 0,
                              const size_t first_element = 0)
    {
        vk::DescriptorBufferInfo buf_info = {};
        buf_info.setBuffer(buffer.m_vma_buffer_bundle->m_vk_buffer);
        buf_info.setOffset(stride * first_element);
        buf_info.setRange(stride * num_elements);
        m_descriptor_buffer_infos.push_back(buf_info);

        vk::WriteDescriptorSet write_descriptor = {};
        write_descriptor.setDstSet(m_vk_descriptor_set);
        write_descriptor.setDstBinding(static_cast<uint32_t>(binding + HlslDxcCompiler::TShift));
        write_descriptor.setDstArrayElement(static_cast<uint32_t>(i_element));
        write_descriptor.setDescriptorCount(1);
        write_descriptor.setDescriptorType(vk::DescriptorType::eStorageBuffer);
        write_descriptor.setPBufferInfo(&m_descriptor_buffer_infos.back());
        m_write_descriptor_set.push_back(write_descriptor);
        return *this;
    }

    DescriptorSet &
    set_t_texture(const size_t binding, const Texture & texture, const size_t i_texture = 0)
    {
        vk::DescriptorImageInfo image_info = {};
        image_info.setImageLayout(texture.m_vk_image_layout);
        image_info.setImageView(texture.m_vk_image_view.get());
        m_descriptor_image_infos.push_back(image_info);

        vk::WriteDescriptorSet write_descriptor = {};
        write_descriptor.setDstSet(m_vk_descriptor_set);
        write_descriptor.setDstBinding(static_cast<uint32_t>(binding + HlslDxcCompiler::TShift));
        write_descriptor.setDstArrayElement(static_cast<uint32_t>(i_texture));
        write_descriptor.setDescriptorCount(1);
        write_descriptor.setDescriptorType(vk::DescriptorType::eSampledImage);
        write_descriptor.setPBufferInfo(nullptr);
        write_descriptor.setPImageInfo(&m_descriptor_image_infos.back());
        m_write_descriptor_set.push_back(write_descriptor);
        return *this;
    }

    DescriptorSet &
    set_u_rw_texture(const size_t binding, const Texture & texture, const size_t i_element = 0)
    {
        vk::DescriptorImageInfo image_info = {};
        image_info.setImageLayout(texture.m_vk_image_layout);
        image_info.setImageView(texture.m_vk_image_view.get());
        m_descriptor_image_infos.push_back(image_info);

        vk::WriteDescriptorSet write_descriptor = {};
        write_descriptor.setDstSet(m_vk_descriptor_set);
        write_descriptor.setDstBinding(static_cast<uint32_t>(binding + HlslDxcCompiler::UShift));
        write_descriptor.setDstArrayElement(static_cast<uint32_t>(i_element));
        write_descriptor.setDescriptorCount(1);
        write_descriptor.setDescriptorType(vk::DescriptorType::eStorageImage);
        write_descriptor.setPBufferInfo(nullptr);
        write_descriptor.setPImageInfo(&m_descriptor_image_infos.back());
        m_write_descriptor_set.push_back(write_descriptor);
        return *this;
    }

    DescriptorSet &
    set_u_rw_structured_buffer(const size_t binding,
                               const Buffer & buffer,
                               const size_t stride,
                               const size_t num_elements,
                               const size_t i_element     = 0,
                               const size_t first_element = 0)
    {
        vk::DescriptorBufferInfo buf_info = {};
        buf_info.setBuffer(buffer.m_vma_buffer_bundle->m_vk_buffer);
        buf_info.setOffset(stride * first_element);
        buf_info.setRange(stride * num_elements);
        m_descriptor_buffer_infos.push_back(buf_info);

        vk::WriteDescriptorSet write_descriptor = {};
        write_descriptor.setDstSet(m_vk_descriptor_set);
        write_descriptor.setDstBinding(static_cast<uint32_t>(binding + HlslDxcCompiler::UShift));
        write_descriptor.setDstArrayElement(static_cast<uint32_t>(i_element));
        write_descriptor.setDescriptorCount(1);
        write_descriptor.setDescriptorType(vk::DescriptorType::eStorageBuffer);
        write_descriptor.setPBufferInfo(&m_descriptor_buffer_infos.back());
        write_descriptor.setPImageInfo(nullptr);
        m_write_descriptor_set.push_back(write_descriptor);
        return *this;
    }

    DescriptorSet &
    set_s_sampler(const size_t binding, const Sampler & sampler)
    {
        vk::DescriptorImageInfo image_info = {};
        image_info.setSampler(sampler.m_vk_sampler.get());
        m_descriptor_image_infos.push_back(image_info);

        vk::WriteDescriptorSet write_descriptor = {};
        write_descriptor.setDstSet(m_vk_descriptor_set);
        write_descriptor.setDstBinding(static_cast<uint32_t>(binding + HlslDxcCompiler::SShift));
        write_descriptor.setDstArrayElement(0);
        write_descriptor.setDescriptorCount(1);
        write_descriptor.setDescriptorType(vk::DescriptorType::eSampler);
        write_descriptor.setPBufferInfo(nullptr);
        write_descriptor.setPImageInfo(&m_descriptor_image_infos.back());
        m_write_descriptor_set.push_back(write_descriptor);
        return *this;
    }

    void
    update()
    {
        m_device->m_vk_ldevice->updateDescriptorSets(m_write_descriptor_set, nullptr);

        m_descriptor_buffer_infos.clear();
        m_descriptor_image_infos.clear();
        m_descriptor_accel_infos.clear();
        m_write_descriptor_set.clear();
    }
};
} // namespace VKA_NAME
#endif