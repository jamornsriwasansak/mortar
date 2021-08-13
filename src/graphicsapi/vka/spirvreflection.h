#pragma once

#include "../gpcommontypes/enums.h"
#include "graphicsapi/shadersrc.h"

#include <spirv_reflect.h>
#include <vulkan/vulkan.hpp>

#include <string>
#include <vector>

struct VkReflectionResult
{
    std::vector<vk::VertexInputBindingDescription>           m_vertex_input_binding_descriptions;
    std::vector<vk::VertexInputAttributeDescription>         m_vertex_input_attribs;
    std::vector<std::string>                                 m_vertex_input_attrib_names;
    std::vector<std::vector<vk::DescriptorSetLayoutBinding>> m_descriptor_set_bindings;
    std::vector<std::vector<std::string>>                    m_descriptor_set_binding_names;
    std::vector<vk::PushConstantRange>                       m_push_constant_ranges;
    std::vector<vk::ShaderStageFlagBits>                     m_shader_stage_flags;
    std::vector<std::string>                                 m_attachment_names;
    std::vector<uint32_t>                                    m_attachment_locations;
    std::vector<vk::Format>                                  m_attachment_formats;
};

struct SpirvReflector
{
    SpirvReflector() {}

    template <typename ShaderStageEnum>
    VkReflectionResult
    reflect(const std::vector<TShaderSrc<ShaderStageEnum>> & shader_srcs,
            const std::vector<std::vector<uint32_t>> &       spirv_codes)
    {
        assert(shader_srcs.size() == spirv_codes.size());

        VkReflectionResult result;
        result.m_shader_stage_flags.resize(spirv_codes.size());

        std::map<std::pair<uint32_t, uint32_t>, std::pair<vk::DescriptorSetLayoutBinding, std::string>> vk_bindings;

        // we iterate through reflected results and src_infos
        // then create vertex_input info, push_constant_blocks
        // and a map from (set, location) to descriptor_set
        for (size_t i_spirv_code = 0; i_spirv_code < spirv_codes.size(); i_spirv_code++)
        {
            Logger::Info(__FUNCTION__ " reflecting shader " +
                             shader_srcs[i_spirv_code].m_file_path.string() + " from stage ",
                         static_cast<int>(shader_srcs[i_spirv_code].m_shader_stage));

            vk::ShaderStageFlagBits shader_stage =
                static_cast<vk::ShaderStageFlagBits>(shader_srcs[i_spirv_code].m_shader_stage);
            result.m_shader_stage_flags[i_spirv_code] = shader_stage;

            spv_reflect::ShaderModule shader_module(spirv_codes[i_spirv_code]);

            // reflect push constants
            reflect_push_constant_blocks(&result,
                                         shader_module,
                                         shader_srcs[i_spirv_code].m_file_path.string());

            // reflect in map of descriptor set layout bindings
            reflect_descriptor_set(&vk_bindings,
                                   shader_module,
                                   shader_stage,
                                   shader_srcs[i_spirv_code].m_file_path.string());

            // reflect vertex shader
            if (shader_stage == vk::ShaderStageFlagBits::eVertex)
            {
                reflect_vertex_input_info(&result, shader_module);
            }

            // reflect fragment shader output and input
            if (shader_stage == vk::ShaderStageFlagBits::eFragment)
            {
                uint32_t num_color_attachments;
                if (shader_module.EnumerateOutputVariables(&num_color_attachments, nullptr) != SPV_REFLECT_RESULT_SUCCESS)
                {
                    Logger::Error<true>(
                        __FUNCTION__ " spirv-reflect failed to fetch num color attachments");
                }
                std::vector<SpvReflectInterfaceVariable *> attachments(num_color_attachments);
                if (shader_module.EnumerateOutputVariables(&num_color_attachments, attachments.data()) !=
                    SPV_REFLECT_RESULT_SUCCESS)
                {
                    Logger::Error<true>(__FUNCTION__ " spirv-reflect failed to write attachments");
                }

                result.m_attachment_formats.resize(num_color_attachments);
                result.m_attachment_locations.resize(num_color_attachments);
                result.m_attachment_names.resize(num_color_attachments);
                for (uint32_t i_color_attachment = 0; i_color_attachment < num_color_attachments; i_color_attachment++)
                {
                    result.m_attachment_formats[i_color_attachment] =
                        static_cast<vk::Format>(attachments[i_color_attachment]->format);
                    result.m_attachment_locations[i_color_attachment] =
                        attachments[i_color_attachment]->location;
                    result.m_attachment_names[i_color_attachment] = attachments[i_color_attachment]->name;
                }
            }
        }

        // in this step, we want to iterate through all set
        // previously our information is stored (set, location)
        // but we want only sets so we have to do some trick

        // first we list all the sets
        uint32_t max_set = 0;
        for (auto & index_pair_binding_info : vk_bindings)
        {
            const uint32_t i_set = index_pair_binding_info.first.first;
            max_set              = std::max(i_set, max_set);
        }
        uint32_t num_set = max_set + 1;
        result.m_descriptor_set_bindings.resize(num_set);
        result.m_descriptor_set_binding_names.resize(num_set);

        // create descriptor set layout for all possible sets
        for (uint32_t i_set = 0; i_set < num_set; i_set++)
        {
            // generate descriptor_set_layout_bindings given a unique set index
            for (auto & index_pair_binding_info : vk_bindings)
            {
                const auto & pair_set_binding = index_pair_binding_info.first;
                if (pair_set_binding.first == i_set)
                {
                    const auto & binding = pair_set_binding.second;

                    const auto & info                  = index_pair_binding_info.second;
                    const auto & vk_set_layout_binding = info.first;
                    const auto & name                  = info.second;

                    result.m_descriptor_set_bindings[i_set].emplace_back(vk_set_layout_binding);
                    result.m_descriptor_set_binding_names[i_set].emplace_back(name);
                }
            }
        }

        return result;
    }

private:
    void
    reflect_vertex_input_info(VkReflectionResult * compile_result, spv_reflect::ShaderModule & vs_shader_module) const
    {
        // enumerate interface variable
        uint32_t input_var_count = 0;
        if (vs_shader_module.EnumerateInputVariables(&input_var_count, nullptr) != SPV_REFLECT_RESULT_SUCCESS)
        {
            Logger::Error<true>(__FUNCTION__ "spirv-reflect failed to fetch num input variables");
        }
        std::vector<SpvReflectInterfaceVariable *> input_vars(input_var_count);
        if (vs_shader_module.EnumerateInputVariables(&input_var_count, input_vars.data()) != SPV_REFLECT_RESULT_SUCCESS)
        {
            Logger::Error<true>(__FUNCTION__ "spirv-reflect failed to write input variables");
        }

        // create vertex input attrib
        for (uint32_t i_var = 0; i_var < input_var_count; i_var++)
        {
            auto & input_var = input_vars[i_var];
            if (input_var->built_in == -1)
            {
                // we assume that the data is always packed
                vk::VertexInputAttributeDescription vertex_input_attrib;
                vertex_input_attrib.setBinding(0u);
                vertex_input_attrib.setLocation(input_var->location);
                vertex_input_attrib.setFormat(static_cast<vk::Format>(input_var->format));
                vertex_input_attrib.setOffset(0u);
                compile_result->m_vertex_input_attribs.push_back(vertex_input_attrib);
                compile_result->m_vertex_input_attrib_names.push_back(std::string(input_var->name));
            }
        }
    }

    void
    reflect_push_constant_blocks(VkReflectionResult *              compile_result,
                                 const spv_reflect::ShaderModule & shader_module,
                                 const std::string &               source_name) const
    {
        // push constants
        uint32_t num_push_constant_blocks = 0;
        if (shader_module.EnumeratePushConstantBlocks(&num_push_constant_blocks, nullptr) != SPV_REFLECT_RESULT_SUCCESS)
        {
            Logger::Error<true>(__FUNCTION__ " Reflection could not get num constants from " + source_name);
        }
        std::vector<SpvReflectBlockVariable *> push_constants(num_push_constant_blocks);
        if (shader_module.EnumeratePushConstantBlocks(&num_push_constant_blocks, push_constants.data()) !=
            SPV_REFLECT_RESULT_SUCCESS)
        {
            Logger::Error<true>(
                __FUNCTION__ " Reflection could not enumerate push constants from " + source_name);
        }

        for (uint32_t i_push_constant_block = 0; i_push_constant_block < num_push_constant_blocks;
             i_push_constant_block++)
        {
            auto &                push_constant = push_constants[i_push_constant_block];
            vk::PushConstantRange vk_push_constant;
            {
                vk_push_constant.setOffset(push_constant->offset);
                vk_push_constant.setSize(push_constant->size);
                vk_push_constant.setStageFlags(vk::ShaderStageFlagBits::eVertex);
            }

            // check overlapping
            bool is_new_push_constant = true;
            for (uint32_t j = 0; j < compile_result->m_push_constant_ranges.size(); j++)
            {
                uvec2 range0 = uvec2(compile_result->m_push_constant_ranges[j].offset,
                                     compile_result->m_push_constant_ranges[j].offset +
                                         compile_result->m_push_constant_ranges[j].size);
                uvec2 range1 =
                    uvec2(vk_push_constant.offset, vk_push_constant.offset + vk_push_constant.size);

                if (range0 == range1)
                {
                    is_new_push_constant = false;
                }
                else if (range0[0] <= range1[1] && range1[0] <= range0[1])
                {
                    Logger::Error<true>(
                        __FUNCTION__ " Reflection encounter overlapped push constant range in " + source_name);
                }
            }
            if (is_new_push_constant)
            {
                compile_result->m_push_constant_ranges.push_back(vk_push_constant);
            }
        }
    }

    void
    reflect_descriptor_set(std::map<std::pair<uint32_t, uint32_t>, std::pair<vk::DescriptorSetLayoutBinding, std::string>> * vk_bindings,
                           const spv_reflect::ShaderModule & shader_module,
                           const vk::ShaderStageFlagBits     vk_shader_stage,
                           const std::string &               source_name) const
    {
        // descriptor sets
        uint32_t num_descriptor_sets = 0;
        if (shader_module.EnumerateDescriptorSets(&num_descriptor_sets, nullptr) != SPV_REFLECT_RESULT_SUCCESS)
        {
            Logger::Error<true>(
                __FUNCTION__ " Reflection failed to fetch num descriptor sets from " + source_name);
        }
        std::vector<SpvReflectDescriptorSet *> descriptor_sets(num_descriptor_sets);
        if (shader_module.EnumerateDescriptorSets(&num_descriptor_sets, descriptor_sets.data()) != SPV_REFLECT_RESULT_SUCCESS)
        {
            Logger::Error<true>(
                __FUNCTION__ " Reflection failed to write num descriptor sets from " + source_name);
        }
        for (uint32_t set_cnt = 0; set_cnt < num_descriptor_sets; set_cnt++)
        {
            auto & reflected_sets = descriptor_sets[set_cnt];
            for (uint32_t binding_cnt = 0; binding_cnt < reflected_sets->binding_count; binding_cnt++)
            {
                const auto & desc_binding = reflected_sets->bindings[binding_cnt];

                const uint32_t i_set     = reflected_sets->set;
                const uint32_t i_binding = reflected_sets->bindings[binding_cnt]->binding;
                const char *   name      = reflected_sets->bindings[binding_cnt]->name;
                const auto     key       = std::make_pair(i_set, i_binding);

                const auto find_result = vk_bindings->find(key);
                if (find_result == vk_bindings->end())
                {
                    // could not find binding. create a new one
                    vk::DescriptorSetLayoutBinding set_layout_binding;
                    set_layout_binding.setBinding(desc_binding->binding);
                    set_layout_binding.setDescriptorCount(desc_binding->count); // TODO:: check if this is correct
                    set_layout_binding.setDescriptorType(
                        static_cast<vk::DescriptorType>(desc_binding->descriptor_type));
                    set_layout_binding.setStageFlags(vk_shader_stage);
                    set_layout_binding.setPImmutableSamplers(nullptr);

                    (*vk_bindings)[key] = std::make_pair(set_layout_binding, name);
                }
                else
                {
                    // fetch value from the pair of (key, value)
                    auto & value = find_result->second;

                    // update stage flags
                    vk::DescriptorSetLayoutBinding & set_layout_binding = value.first;
                    if (set_layout_binding.descriptorCount != desc_binding->count ||
                        set_layout_binding.descriptorType !=
                            static_cast<vk::DescriptorType>(desc_binding->descriptor_type))
                    {
                        Logger::Error<true>(
                            __FUNCTION__ " Reflection found discreprancy in descriptor from " + source_name);
                    }
                    else
                    {
                        set_layout_binding.setStageFlags(set_layout_binding.stageFlags | vk_shader_stage);
                    }
                }
            }
        }
    }
};