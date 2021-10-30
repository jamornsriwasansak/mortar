#pragma once

#include "vkacommon.h"
#include "vkadevice.h"

#include "../shadercompiler/hlsldxccompiler.h"
#include "rhi/commontypes/rhishadersrc.h"
#include "spirvreflection.h"

namespace VKA_NAME
{
struct RayTracingHitGroup
{
    std::optional<size_t> m_closest_hit_id;
    std::optional<size_t> m_any_hit_id;
    std::optional<size_t> m_intersect_id;
};

struct RayTracingPipelineConfig
{
    RayTracingPipelineConfig() {}

    std::vector<ShaderSrc> m_shader_srcs;
    std::vector<RayTracingHitGroup> m_hit_groups;
    // std::vector<vk::PipelineShaderStageCreateInfo>      m_shader_stages;
    // std::vector<vk::RayTracingShaderGroupCreateInfoKHR> m_shader_groups;

    size_t
    add_shader(const ShaderSrc & shader_src)
    {
        m_shader_srcs.push_back(shader_src);
        return m_shader_srcs.size() - 1;
    }

    size_t
    add_hit_group(const RayTracingHitGroup & hit_group)
    {
        m_hit_groups.push_back(hit_group);
        return m_hit_groups.size() - 1;
    }

    size_t
    get_num_shaders(const ShaderStageEnum shader_stage) const
    {
        size_t result = 0;
        for (const ShaderSrc & src : m_shader_srcs)
        {
            if (src.m_shader_stage == shader_stage)
            {
                result += 1;
            }
        }
        return result;
    }
};

struct RayTracingPipeline
{
    vk::UniquePipeline m_vk_pipeline;
    vk::UniquePipelineLayout m_vk_pipeline_layout;
    std::vector<vk::UniqueDescriptorSetLayout> m_vk_descriptor_set_layouts;
    size_t m_num_raygens;
    size_t m_num_misses;
    size_t m_num_hit_groups;

    RayTracingPipeline() {}

    RayTracingPipeline(const Device * device,
                       const RayTracingPipelineConfig & rt_lib,
                       [[maybe_unused]] const size_t attribute_size,
                       [[maybe_unused]] const size_t payload_size,
                       const size_t recursion_depth = 1,
                       const std::string & name     = "")
    : m_num_raygens(rt_lib.get_num_shaders(ShaderStageEnum::RayGen)),
      m_num_misses(rt_lib.get_num_shaders(ShaderStageEnum::Miss)),
      m_num_hit_groups(rt_lib.m_hit_groups.size())
    {
        // compile all shader srcs
        HlslDxcCompiler hlsl_compiler;
        std::vector<std::vector<uint32_t>> spirv_codes(rt_lib.m_shader_srcs.size());
        std::vector<vk::UniqueShaderModule> shader_module(rt_lib.m_shader_srcs.size());
        for (size_t i = 0; i < rt_lib.m_shader_srcs.size(); i++)
        {
            // spirv code
            spirv_codes[i] = hlsl_compiler.compile_as_spirv(rt_lib.m_shader_srcs[i]);

            // create shader module
            vk::ShaderModuleCreateInfo shader_module_ci;
            shader_module_ci.setCode(spirv_codes[i]);
            shader_module[i] = device->m_vk_ldevice->createShaderModuleUnique(shader_module_ci);
        }

        // reflection
        SpirvReflector spirv_reflector;
        VkReflectionResult reflection = spirv_reflector.reflect(rt_lib.m_shader_srcs, spirv_codes);

        // descriptor layout
        Logger::Info(__FUNCTION__ " creating pipeline layout");
        m_vk_descriptor_set_layouts.clear();
        for (auto & bindings : reflection.m_descriptor_set_bindings)
        {
            vk::DescriptorSetLayoutCreateInfo desc_layout_info_ci;
            desc_layout_info_ci.setBindings(bindings);
            m_vk_descriptor_set_layouts.emplace_back(
                device->m_vk_ldevice->createDescriptorSetLayoutUnique(desc_layout_info_ci));
        }

        // pipeline create info
        std::vector<vk::DescriptorSetLayout> descriptor_layouts = vk::uniqueToRaw(m_vk_descriptor_set_layouts);
        vk::PipelineLayoutCreateInfo pipeline_layout_ci = {};
        pipeline_layout_ci.setPSetLayouts(descriptor_layouts.data());
        pipeline_layout_ci.setSetLayoutCount(static_cast<uint32_t>(descriptor_layouts.size()));
        pipeline_layout_ci.setPushConstantRangeCount(0);
        pipeline_layout_ci.setPPushConstantRanges(nullptr);
        m_vk_pipeline_layout = device->m_vk_ldevice->createPipelineLayoutUnique(pipeline_layout_ci);

        // shader stage create infos
        std::vector<vk::PipelineShaderStageCreateInfo> shader_stage_cis(rt_lib.m_shader_srcs.size());
        for (size_t i = 0; i < rt_lib.m_shader_srcs.size(); i++)
        {
            vk::PipelineShaderStageCreateInfo & shader_stage_ci = shader_stage_cis[i];
            shader_stage_ci.setModule(shader_module[i].get());
            shader_stage_ci.setStage(static_cast<vk::ShaderStageFlagBits>(rt_lib.m_shader_srcs[i].m_shader_stage));
            shader_stage_ci.setPName(rt_lib.m_shader_srcs[i].m_entry.c_str());
        }

        // shader group create infos pass for raygen shader and miss shader
        std::vector<vk::RayTracingShaderGroupCreateInfoKHR> shader_group_cis(
            m_num_raygens + m_num_misses + m_num_hit_groups);
        size_t i_group = 0;
        for (size_t i = 0; i < rt_lib.m_shader_srcs.size(); i++)
        {
            // id is the same as array index
            const uint32_t shader_id = static_cast<uint32_t>(i);
            const auto & shader_src  = rt_lib.m_shader_srcs[i];
            if (shader_src.m_shader_stage == ShaderStageEnum::RayGen ||
                shader_src.m_shader_stage == ShaderStageEnum::Miss)
            {
                vk::RayTracingShaderGroupCreateInfoKHR & shader_group_ci = shader_group_cis[i_group];
                shader_group_ci.setType(vk::RayTracingShaderGroupTypeKHR::eGeneral);
                shader_group_ci.setGeneralShader(shader_id);
                shader_group_ci.setAnyHitShader(VK_SHADER_UNUSED_KHR);
                shader_group_ci.setClosestHitShader(VK_SHADER_UNUSED_KHR);
                shader_group_ci.setIntersectionShader(VK_SHADER_UNUSED_KHR);
                i_group++;
            }
        }

        // shader group create infos pass for hitgroup
        for (const auto & hit_group : rt_lib.m_hit_groups)
        {
            vk::RayTracingShaderGroupCreateInfoKHR & shader_group_ci = shader_group_cis[i_group];
            shader_group_ci.setGeneralShader(VK_SHADER_UNUSED_KHR);
            shader_group_ci.setAnyHitShader(VK_SHADER_UNUSED_KHR);
            shader_group_ci.setClosestHitShader(VK_SHADER_UNUSED_KHR);
            shader_group_ci.setIntersectionShader(VK_SHADER_UNUSED_KHR);
            if (hit_group.m_any_hit_id.has_value())
            {
                shader_group_ci.setAnyHitShader(static_cast<uint32_t>(hit_group.m_any_hit_id.value()));
            }
            if (hit_group.m_closest_hit_id.has_value())
            {
                shader_group_ci.setClosestHitShader(static_cast<uint32_t>(hit_group.m_closest_hit_id.value()));
            }
            if (hit_group.m_intersect_id.has_value())
            {
                shader_group_ci.setIntersectionShader(static_cast<uint32_t>(hit_group.m_intersect_id.value()));
            }
            shader_group_ci.setType(hit_group.m_intersect_id.has_value()
                                        ? vk::RayTracingShaderGroupTypeKHR::eProceduralHitGroup
                                        : vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup);

            i_group++;
        }
        assert(i_group == shader_group_cis.size());

        // create pipeline
        vk::RayTracingPipelineCreateInfoKHR ray_pipeline_ci = {};
        ray_pipeline_ci.setStages(shader_stage_cis);
        ray_pipeline_ci.setGroups(shader_group_cis);
        ray_pipeline_ci.setMaxPipelineRayRecursionDepth(static_cast<uint32_t>(recursion_depth));
        ray_pipeline_ci.setLayout(m_vk_pipeline_layout.get());
        auto result = device->m_vk_ldevice->createRayTracingPipelineKHRUnique(nullptr, nullptr, ray_pipeline_ci);
        VKCK(result.result);
        m_vk_pipeline = std::move(result.value);
        device->name_vkhpp_object<vk::Pipeline, vk::Pipeline::CType>(m_vk_pipeline.get(), name);
    }
};

struct RayTracingShaderTable
{
    struct VmaSbtBundle
    {
        VmaAllocator m_vma_allocator;
        VmaAllocation m_vma_allocation;
        VkBuffer m_vk_buffer;
    };

    struct VmaSbtBundleDeleter
    {
        VmaSbtBundleDeleter() {}
        void
        operator()(VmaSbtBundle bundle)
        {
            vmaDestroyBuffer(bundle.m_vma_allocator, bundle.m_vk_buffer, bundle.m_vma_allocation);
        }
    };

    UniqueVarHandle<VmaSbtBundle, VmaSbtBundleDeleter> m_vma_sbt_bundle;
    vk::StridedDeviceAddressRegionKHR m_raygen_device_region;
    vk::StridedDeviceAddressRegionKHR m_miss_device_region;
    vk::StridedDeviceAddressRegionKHR m_hitgroup_device_region;
    vk::StridedDeviceAddressRegionKHR m_callable_device_region;

    RayTracingShaderTable() {}

    RayTracingShaderTable(const Device * device,
                          const RayTracingPipeline & pipeline,
                          const std::string & name = "")
    {
        // compute shader handle size
        const auto full_properties2 =
            device->m_vk_pdevice.getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceRayTracingPropertiesNV>();
        const auto & rt_properties = full_properties2.get<vk::PhysicalDeviceRayTracingPropertiesNV>();
        const uint32_t handle_size      = rt_properties.shaderGroupHandleSize;
        const uint32_t handle_alignment = rt_properties.shaderGroupBaseAlignment;

        // get raytracing shader group handle
        const uint32_t num_groups =
            static_cast<uint32_t>(pipeline.m_num_raygens + pipeline.m_num_misses + pipeline.m_num_hit_groups);
        const uint32_t raygen_size    = static_cast<uint32_t>(handle_size * pipeline.m_num_raygens);
        const uint32_t miss_size      = static_cast<uint32_t>(handle_size * pipeline.m_num_misses);
        const uint32_t hit_group_size = static_cast<uint32_t>(handle_size * pipeline.m_num_hit_groups);
        const uint32_t aligned_raygen_size   = round_up(raygen_size, handle_alignment);
        const uint32_t aligned_miss_size     = round_up(miss_size, handle_alignment);
        const uint32_t aligned_hitgroup_size = round_up(hit_group_size, handle_alignment);
        const uint32_t data_size = aligned_raygen_size + aligned_miss_size + aligned_hitgroup_size;
        vk::Pipeline rt_pipeline = pipeline.m_vk_pipeline.get();
        std::vector<uint8_t> rt_group_handle(data_size);
        VKCK(device->m_vk_ldevice->getRayTracingShaderGroupHandlesKHR(rt_pipeline,
                                                                      0u,
                                                                      num_groups,
                                                                      data_size,
                                                                      rt_group_handle.data()));

        // buffer create info
        vk::BufferCreateInfo buffer_ci_tmp;
        buffer_ci_tmp.setUsage(vk::BufferUsageFlagBits::eShaderDeviceAddress |
                               vk::BufferUsageFlagBits::eShaderBindingTableKHR);
        buffer_ci_tmp.setSize(data_size);
        VkBufferCreateInfo buffer_ci         = buffer_ci_tmp;
        VmaAllocationCreateInfo vma_alloc_ci = {};
        vma_alloc_ci.usage                   = VmaMemoryUsage::VMA_MEMORY_USAGE_CPU_ONLY;

        // create buffer
        VkBuffer vma_vk_buffer;
        VmaAllocation vma_allocation;
        VmaAllocationInfo vma_alloc_info;
        VKCK(vmaCreateBuffer(*device->m_vma_allocator, &buffer_ci, &vma_alloc_ci, &vma_vk_buffer, &vma_allocation, &vma_alloc_info));

        // bundle
        VmaSbtBundle sbt_bundle;
        sbt_bundle.m_vk_buffer      = vma_vk_buffer;
        sbt_bundle.m_vma_allocation = vma_allocation;
        sbt_bundle.m_vma_allocator  = device->m_vma_allocator.get();
        m_vma_sbt_bundle.m_value    = sbt_bundle;

        void * mapped_data = vma_alloc_info.pMappedData;
        if (vma_alloc_info.pMappedData == nullptr)
        {
            vmaMapMemory(*device->m_vma_allocator, vma_allocation, &mapped_data);
        }
        // copy raygen
        uint8_t * write_ptr = reinterpret_cast<uint8_t *>(mapped_data);
        uint8_t * read_ptr  = rt_group_handle.data();
        std::memcpy(write_ptr, read_ptr, raygen_size);
        write_ptr += aligned_raygen_size;
        read_ptr += raygen_size;
        std::memcpy(write_ptr, read_ptr, miss_size);
        write_ptr += aligned_miss_size;
        read_ptr += miss_size;
        std::memcpy(write_ptr, read_ptr, hit_group_size);
        write_ptr += aligned_hitgroup_size;
        read_ptr += hit_group_size;

        if (vma_alloc_info.pMappedData == nullptr)
        {
            vmaUnmapMemory(*device->m_vma_allocator, vma_allocation);
        }

        // set name for buffer
        device->name_vkhpp_object<vk::Buffer, vk::Buffer::CType>(vk::Buffer(vma_vk_buffer), name);

        vk::BufferDeviceAddressInfo device_address_info;
        device_address_info.setBuffer(vk::Buffer(vma_vk_buffer));
        vk::DeviceAddress buffer_address = device->m_vk_ldevice->getBufferAddress(device_address_info);

        m_raygen_device_region.setDeviceAddress(buffer_address);
        m_raygen_device_region.setStride(handle_size);
        m_raygen_device_region.setSize(raygen_size);
        buffer_address += aligned_raygen_size;

        // miss
        if (pipeline.m_num_misses > 0)
        {
            m_miss_device_region.setDeviceAddress(buffer_address);
            m_miss_device_region.setStride(handle_size);
            m_miss_device_region.setSize(miss_size);
            buffer_address += aligned_miss_size;
        }
        else
        {
            m_miss_device_region.setDeviceAddress(0);
            m_miss_device_region.setStride(0);
            m_miss_device_region.setSize(0);
        }

        // hit group
        if (pipeline.m_num_hit_groups > 0)
        {
            m_hitgroup_device_region.setDeviceAddress(buffer_address);
            m_hitgroup_device_region.setStride(handle_size);
            m_hitgroup_device_region.setSize(hit_group_size);
            buffer_address += aligned_hitgroup_size;
        }
        else
        {
            m_hitgroup_device_region.setDeviceAddress(0);
            m_hitgroup_device_region.setStride(0);
            m_hitgroup_device_region.setSize(0);
        }

        m_callable_device_region.setDeviceAddress(0);
        m_callable_device_region.setSize(0);
        m_callable_device_region.setStride(0);
    }
};

} // namespace VKA_NAME
