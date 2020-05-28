#pragma once

#include <vulkan/vulkan.hpp>
#include <glslang/Public/ShaderLang.h>
#include <SPIRV/GlslangToSpv.h>
#include <StandAlone/DirStackFileIncluder.h>

#include "gavulkan/buffer.h"

struct ShaderAttributeInfo
{
	uint32_t	m_location = 0;
	std::string	m_name = "";
	std::string	m_type = "";
};

struct ParsedFormatQualifier
{
	size_t			m_num_channels = 0;
	size_t			m_num_bits = 0;
	std::string		m_type = "";
};

struct ShaderUniformInfo
{
	bool					m_is_push_constant;
	uint32_t				m_set = 0;
	uint32_t				m_binding = 0;
	std::string				m_mem_type = "";
	std::string				m_name = "";
	std::string				m_type = "";
	uint32_t				m_size_in_bytes = 0;
	uint32_t				m_num_descriptors = 1;
	ParsedFormatQualifier	m_parsed_format_qualifier = {};

	vk::DescriptorType
	get_descriptor_type() const
	{
		// first we determine with type
		if (m_type == "sampler2D") return vk::DescriptorType::eCombinedImageSampler;
		if (m_type == "image2D") return vk::DescriptorType::eStorageImage;
		if (m_type == "image3D") return vk::DescriptorType::eStorageImage;
		if (m_type == "uimage2D") return vk::DescriptorType::eStorageImage;
		if (m_type == "uimage3D") return vk::DescriptorType::eStorageImage;
		if (start_with(m_type, "accelerationStructure")) return vk::DescriptorType::eAccelerationStructureKHR;

		// else we guess with mem_type
		if (m_mem_type == "buffer") return vk::DescriptorType::eStorageBuffer;
		if (m_mem_type == "uniform") return vk::DescriptorType::eUniformBuffer;
		THROW("unknown descriptor type type : " + m_type + ", mem_type : " + m_mem_type);
		return vk::DescriptorType(0);
	}
};

struct Shader
{
	vk::UniqueShaderModule										m_vk_shader_module;
	vk::ShaderStageFlagBits										m_vk_shader_stage;
	std::vector<ShaderAttributeInfo>							m_attributes;
	std::vector<ShaderUniformInfo>								m_uniforms;
	std::map<std::pair<size_t, size_t>, ShaderUniformInfo *>	m_uniforms_set;

	Shader(const std::filesystem::path shader_path,
		   const vk::ShaderStageFlagBits & shader_stage):
		m_vk_shader_module(nullptr),
		m_vk_shader_stage(shader_stage)
	{
		// generate spir-v shader code
		std::string shader_string;
		std::vector<uint32_t> shader_code;
		compile_src(&shader_string,
					&shader_code,
					shader_path.string(),
					shader_stage,
					shader_path,
					true);

		// parse shader string
		shader_string = remove_block_comments(shader_string,
											  { std::make_pair("/*", "*/") });
		shader_string = remove_line_comments(shader_string,
											 { "//", "#" });

		// introspect
		auto introspect_result = introspect(shader_string);
		m_attributes = std::move(introspect_result.m_shader_attribs);
		m_uniforms = std::move(introspect_result.m_shader_uniforms);

		// init m_uniform_from_uniform_name
		for (ShaderUniformInfo & uniform : m_uniforms)
		{
			m_uniforms_set[std::make_pair(uniform.m_set, uniform.m_binding)] = &uniform;
		}

		// shader module create info
		vk::ShaderModuleCreateInfo shader_module_ci;
		{
			shader_module_ci.setCodeSize(shader_code.size() * sizeof(uint32_t));
			shader_module_ci.setPCode(shader_code.data());
		}

		// create shader module
		m_vk_shader_module = Core::Inst().m_vk_device->createShaderModuleUnique(shader_module_ci);
		m_vk_shader_stage = shader_stage;
	}

	vk::PipelineShaderStageCreateInfo
	get_pipeline_shader_stage_ci() const
	{
		// shader stage create info 
		vk::PipelineShaderStageCreateInfo stage_ci;
		stage_ci.setStage(m_vk_shader_stage);
		stage_ci.setModule(*m_vk_shader_module);
		stage_ci.setPName("main");
		return stage_ci;
	}

	vk::RayTracingShaderGroupCreateInfoKHR
	get_raytracing_shader_group_ci(const uint32_t i_shader) const
	{
		vk::RayTracingShaderGroupCreateInfoKHR rsg_ci;
		rsg_ci.setAnyHitShader(VK_SHADER_UNUSED_KHR);
		rsg_ci.setClosestHitShader(VK_SHADER_UNUSED_KHR);
		rsg_ci.setGeneralShader(VK_SHADER_UNUSED_KHR);
		rsg_ci.setIntersectionShader(VK_SHADER_UNUSED_KHR);

		if (m_vk_shader_stage == vk::ShaderStageFlagBits::eClosestHitKHR)
		{
			rsg_ci.setType(vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup);
			rsg_ci.setClosestHitShader(i_shader);
		}
		else if (m_vk_shader_stage == vk::ShaderStageFlagBits::eAnyHitKHR)
		{
			rsg_ci.setType(vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup);
			rsg_ci.setAnyHitShader(i_shader);
		}
		else if (m_vk_shader_stage == vk::ShaderStageFlagBits::eRaygenKHR)
		{
			rsg_ci.setType(vk::RayTracingShaderGroupTypeKHR::eGeneral);
			rsg_ci.setGeneralShader(i_shader);
		}
		else if (m_vk_shader_stage == vk::ShaderStageFlagBits::eMissKHR)
		{
			rsg_ci.setType(vk::RayTracingShaderGroupTypeKHR::eGeneral);
			rsg_ci.setGeneralShader(i_shader);
		}
		else
		{
			THROW("unknown raytracing shader type");
		}
		
		return rsg_ci;
	}

	ShaderUniformInfo &
	uniform(const size_t set,
			const size_t binding) const
	{
		std::pair<size_t, size_t> set_binding = std::make_pair(set,
															   binding);
		return *m_uniforms_set.at(set_binding);
	}

private:

	std::optional<ParsedFormatQualifier>
	parse_format_qualifier(const std::string & str)
	{
		size_t i_char = 0;

		// first check num channels
		const std::string rgba_str = "rgba";
		size_t num_channels = 0;
		for (; i_char < str.size(); i_char++)
		{
			if (str[i_char] == rgba_str[i_char])
			{
				num_channels++;
			}
			else
			{
				break;
			}
		}
		if (num_channels == 0)
		{
			return std::nullopt;
		}

		// then check num bits
		const size_t i_num_bits_begin = i_char;
		for (; i_char < str.size(); i_char++)
		{
			if (!std::isdigit(str[i_char]))
			{
				break;
			}
		}
		const size_t i_num_bits_end = i_char;
		if (i_num_bits_end - i_num_bits_begin == 0)
		{
			return std::nullopt;
		}
		const std::string num_bits_str = str.substr(i_num_bits_begin, i_num_bits_end - i_num_bits_begin);
		const size_t num_bits = size_t(std::stoi(num_bits_str));

		// the rest is type
		const std::string type = str.substr(i_num_bits_end);
		if (type != "" &&
			type != "f" &&
			type != "i" &&
			type != "ui")
		{
			THROW("unknown support Format Qualifier : " + type);
		}

		// prepare the result
		ParsedFormatQualifier result;
		{
			result.m_num_channels = num_channels;
			result.m_num_bits = num_bits;
			result.m_type = type;
		}
		return result;
	}

	struct ShaderIntrospectReturnType
	{
		std::vector<ShaderAttributeInfo> m_shader_attribs;
		std::vector<ShaderUniformInfo> m_shader_uniforms;
	};
	ShaderIntrospectReturnType
	introspect(const std::string & source)
	{
		std::vector<ShaderAttributeInfo> shader_attribs;
		std::vector<ShaderUniformInfo> shader_uniforms;
		const std::vector<std::string> tokens = tokenize(source,
														 "{}[](),;=",
														 true);
		for (size_t i_token = 0; i_token < tokens.size(); i_token++)
		{
			if (tokens[i_token] == "layout")
			{
				THROW_ASSERT(tokens[i_token + 1] == "(",
					"expect an equal symbol after an open parenthesis afet keyword \"layout\" instead \"" + tokens[i_token + 1] + "\" was found");

				// move past "layout" and "parenthesis"
				i_token += 2;

				// -1 = unknown
				int location = -1;
				int binding = -1;
				int set = -1;
				bool is_push_constant = false;
				ParsedFormatQualifier pfq = {};
				while (true)
				{
					if (tokens[i_token] == "location")
					{
						THROW_ASSERT(tokens[i_token + 1] == "=",
							"expect an equal symbol after a keyword \"location\" instead \"" + tokens[i_token + 1] + "\" was found");
						THROW_ASSERT(location == -1, "encountered a keyword \"location\" twice");
						try
						{
							location = std::stoi(tokens[i_token + 2]);
						}
						catch (const std::exception)
						{
							location = -1;
						};
						i_token += 3;
					}
					else if (tokens[i_token] == "set")
					{
						THROW_ASSERT(tokens[i_token + 1] == "=",
							"expect an equal symbol after a keyword \"set\" instead \"" + tokens[i_token + 1] + "\" was found");
						THROW_ASSERT(set == -1, "encountered a keyword \"set\" twice");
						set = std::stoi(tokens[i_token + 2]);
						i_token += 3;
					}
					else if (tokens[i_token] == "binding")
					{
						THROW_ASSERT(tokens[i_token + 1] == "=",
							"expect an equal symbol after a keyword \"binding\" instead \"" + tokens[i_token + 1] + "\" was found");
						THROW_ASSERT(binding == -1, "encountered a keyword \"binding\" twice");
						binding = std::stoi(tokens[i_token + 2]);
						i_token += 3;
					}
					else if (tokens[i_token] == "push_constant")
					{
						THROW_ASSERT(tokens[i_token + 1] == "(",
							"expect layout qualifier of push_constant to contain only push_constant");
						THROW_ASSERT(tokens[i_token + 1] == ")",
							"expect layout qualifier of push_constant to contain only push_constant");
						is_push_constant = true;
					}
					else if (tokens[i_token] == ",")
					{
						// move to the next keyword
						i_token += 1;
					}
					else if (tokens[i_token] == ")")
					{
						// end of layout specification
						i_token += 1;
						break;
					}
					else if (auto parsed_format_qualifier = parse_format_qualifier(tokens[i_token]))
					{
						pfq = *parsed_format_qualifier;
						i_token += 1;
					}
					else if (tokens[i_token] == "std430")
					{
						i_token += 1;
					}
					else
					{
						THROW("encounter unknown token in layout specification :\"" + tokens[i_token] + "\"");
					}
				}

				if (tokens[i_token] == "in")
				{
					// attribute
					const std::string type = tokens[i_token + 1];
					const std::string name = tokens[i_token + 2];

					// push back shader attribute
					ShaderAttributeInfo attrib;
					attrib.m_name = name;
					attrib.m_type = type;
					attrib.m_location = static_cast<uint32_t>(location);
					shader_attribs.push_back(attrib);

					i_token += 2;
				}
				else if (tokens[i_token] == "uniform" || tokens[i_token] == "buffer")
				{
					// [uniform | ssbo]   type_name   [var_name | {]
					//      ^token
					const std::string mem_type = tokens[i_token];
					const std::string type = tokens[i_token + 1];

					size_t num_open_bracket = 0;
					uint32_t size_in_bytes = 0;

					i_token += 2;

					while (true)
					{
						// push / pop bracket
						if (tokens[i_token] == "{")
						{
							num_open_bracket += 1;
						}
						else if (tokens[i_token] == "}")
						{
							num_open_bracket -= 1;
							size_in_bytes = div_and_ceil(size_in_bytes, 16u) * 16;
						}
						else
						{
							std::optional<size_t> type_size_in_bytes = get_glsl_type_size_in_bytes(tokens[i_token]);
							if (type_size_in_bytes == 4)
							{
								size_in_bytes += 4u;
							}
							else if (type_size_in_bytes > 4 &&
									 type_size_in_bytes <= 8)
							{
								// should be *vec2
								size_in_bytes = div_and_ceil(size_in_bytes, 8u) * 8u;
								size_in_bytes += static_cast<uint32_t>(type_size_in_bytes.value());
							}
							else if (type_size_in_bytes > 16)
							{
								// should be *vec3, *vec4, *mat2, *mat3, *mat4
								THROW_ASSERT(type_size_in_bytes.value() % 16 == 0,
									"unknown type :" + type + " is not multiple of 16 bytes");
								size_in_bytes = div_and_ceil(size_in_bytes, 16u) * 16u;
								size_in_bytes += static_cast<uint32_t>(type_size_in_bytes.value());
							}
						}

						// break if the we're currently pointing at "var_name" or "{"
						if (num_open_bracket == 0)
						{
							break;
						}

						i_token++;
					}

					// if we're currently pointing at "{", increment the index so that we are pointing at "var_name"
					if (tokens[i_token] == "}")
					{
						i_token++;
					}

					const std::string name = tokens[i_token];

					ShaderUniformInfo uniform;
					uniform.m_is_push_constant = is_push_constant;
					uniform.m_mem_type = mem_type;
					uniform.m_set = set == -1 ? 0 : static_cast<uint32_t>(set);
					uniform.m_binding = static_cast<uint32_t>(binding);
					uniform.m_type = type;
					uniform.m_name = name;
					uniform.m_size_in_bytes = size_in_bytes;
					uniform.m_parsed_format_qualifier = pfq;
					shader_uniforms.push_back(std::move(uniform));
				}
			}
		}
		return { std::move(shader_attribs), std::move(shader_uniforms) };
	}

	TBuiltInResource GetResources()
	{
		TBuiltInResource resources = {};
		resources.maxLights = 32;
		resources.maxClipPlanes = 6;
		resources.maxTextureUnits = 32;
		resources.maxTextureCoords = 32;
		resources.maxVertexAttribs = 64;
		resources.maxVertexUniformComponents = 4096;
		resources.maxVaryingFloats = 64;
		resources.maxVertexTextureImageUnits = 32;
		resources.maxCombinedTextureImageUnits = 80;
		resources.maxTextureImageUnits = 32;
		resources.maxFragmentUniformComponents = 4096;
		resources.maxDrawBuffers = 32;
		resources.maxVertexUniformVectors = 128;
		resources.maxVaryingVectors = 8;
		resources.maxFragmentUniformVectors = 16;
		resources.maxVertexOutputVectors = 16;
		resources.maxFragmentInputVectors = 15;
		resources.minProgramTexelOffset = -8;
		resources.maxProgramTexelOffset = 7;
		resources.maxClipDistances = 8;
		resources.maxComputeWorkGroupCountX = 65535;
		resources.maxComputeWorkGroupCountY = 65535;
		resources.maxComputeWorkGroupCountZ = 65535;
		resources.maxComputeWorkGroupSizeX = 1024;
		resources.maxComputeWorkGroupSizeY = 1024;
		resources.maxComputeWorkGroupSizeZ = 64;
		resources.maxComputeUniformComponents = 1024;
		resources.maxComputeTextureImageUnits = 16;
		resources.maxComputeImageUniforms = 8;
		resources.maxComputeAtomicCounters = 8;
		resources.maxComputeAtomicCounterBuffers = 1;
		resources.maxVaryingComponents = 60;
		resources.maxVertexOutputComponents = 64;
		resources.maxGeometryInputComponents = 64;
		resources.maxGeometryOutputComponents = 128;
		resources.maxFragmentInputComponents = 128;
		resources.maxImageUnits = 8;
		resources.maxCombinedImageUnitsAndFragmentOutputs = 8;
		resources.maxCombinedShaderOutputResources = 8;
		resources.maxImageSamples = 0;
		resources.maxVertexImageUniforms = 0;
		resources.maxTessControlImageUniforms = 0;
		resources.maxTessEvaluationImageUniforms = 0;
		resources.maxGeometryImageUniforms = 0;
		resources.maxFragmentImageUniforms = 8;
		resources.maxCombinedImageUniforms = 8;
		resources.maxGeometryTextureImageUnits = 16;
		resources.maxGeometryOutputVertices = 256;
		resources.maxGeometryTotalOutputComponents = 1024;
		resources.maxGeometryUniformComponents = 1024;
		resources.maxGeometryVaryingComponents = 64;
		resources.maxTessControlInputComponents = 128;
		resources.maxTessControlOutputComponents = 128;
		resources.maxTessControlTextureImageUnits = 16;
		resources.maxTessControlUniformComponents = 1024;
		resources.maxTessControlTotalOutputComponents = 4096;
		resources.maxTessEvaluationInputComponents = 128;
		resources.maxTessEvaluationOutputComponents = 128;
		resources.maxTessEvaluationTextureImageUnits = 16;
		resources.maxTessEvaluationUniformComponents = 1024;
		resources.maxTessPatchComponents = 120;
		resources.maxPatchVertices = 32;
		resources.maxTessGenLevel = 64;
		resources.maxViewports = 16;
		resources.maxVertexAtomicCounters = 0;
		resources.maxTessControlAtomicCounters = 0;
		resources.maxTessEvaluationAtomicCounters = 0;
		resources.maxGeometryAtomicCounters = 0;
		resources.maxFragmentAtomicCounters = 8;
		resources.maxCombinedAtomicCounters = 8;
		resources.maxAtomicCounterBindings = 1;
		resources.maxVertexAtomicCounterBuffers = 0;
		resources.maxTessControlAtomicCounterBuffers = 0;
		resources.maxTessEvaluationAtomicCounterBuffers = 0;
		resources.maxGeometryAtomicCounterBuffers = 0;
		resources.maxFragmentAtomicCounterBuffers = 1;
		resources.maxCombinedAtomicCounterBuffers = 1;
		resources.maxAtomicCounterBufferSize = 16384;
		resources.maxTransformFeedbackBuffers = 4;
		resources.maxTransformFeedbackInterleavedComponents = 64;
		resources.maxCullDistances = 8;
		resources.maxCombinedClipAndCullDistances = 8;
		resources.maxSamples = 4;
		resources.limits.nonInductiveForLoops = true;
		resources.limits.whileLoops = true;
		resources.limits.doWhileLoops = true;
		resources.limits.generalUniformIndexing = true;
		resources.limits.generalAttributeMatrixVectorIndexing = true;
		resources.limits.generalVaryingIndexing = true;
		resources.limits.generalSamplerIndexing = true;
		resources.limits.generalVariableIndexing = true;
		resources.limits.generalConstantMatrixVectorIndexing = true;
		return resources;
	}

	void
	compile_src(std::string * output_source,
				std::vector<uint32_t> * spirv,
				const std::string & source_name,
				vk::ShaderStageFlagBits vk_shader_stage,
				const std::filesystem::path & path,
				bool optimize = false)
	{
		const glslang::EshTargetClientVersion esh_target_version = glslang::EshTargetClientVersion(Core::Inst().m_vk_api_made_version);
		const glslang::EShTargetLanguageVersion esh_target_spv_version = glslang::EShTargetSpv_1_3;

		// ref: https://forestsharp.com/glslang-cpp/
		// ref: https://github.com/EQMG/Acid/blob/master/Sources/Graphics/Pipelines/Shader.cpp
		static const std::map<vk::ShaderStageFlagBits, EShLanguage> map_from_vk_stage =
		{
			{vk::ShaderStageFlagBits::eVertex, EShLanguage::EShLangVertex},
			{vk::ShaderStageFlagBits::eFragment, EShLanguage::EShLangFragment},
			{vk::ShaderStageFlagBits::eGeometry, EShLanguage::EShLangGeometry},
			{vk::ShaderStageFlagBits::eCompute, EShLanguage::EShLangCompute},
			{vk::ShaderStageFlagBits::eRaygenKHR, EShLanguage::EShLangRayGen},
			{vk::ShaderStageFlagBits::eAnyHitKHR, EShLanguage::EShLangAnyHit},
			{vk::ShaderStageFlagBits::eClosestHitKHR, EShLanguage::EShLangClosestHit},
			{vk::ShaderStageFlagBits::eMissKHR, EShLanguage::EShLangMiss},
		};

		const EShLanguage esh_stage = map_from_vk_stage.at(vk_shader_stage);
		const EShMessages esh_messages = EShMessages(EShMessages::EShMsgSpvRules |
													 EShMessages::EShMsgVulkanRules |
													 EShMessages::EShMsgDefault |
													 EShMessages::EShMsgDebugInfo);
		const std::string source = load_file(path);
		const char * source_c_str = source.c_str();

		glslang::TShader shader(esh_stage);
		DirStackFileIncluder includer;
		// "shaders" folder
		includer.pushExternalLocalDirectory((std::filesystem::current_path() / "shaders").string());

		shader.setStrings(&(source_c_str), 1);
		shader.setEnvInput(glslang::EShSourceGlsl,
						   esh_stage,
						   glslang::EShClientVulkan,
						   Core::Inst().m_vk_api_version);
		shader.setEnvClient(glslang::EShClientVulkan,
							esh_target_version);
		shader.setEnvTarget(glslang::EshTargetSpv,
							esh_target_spv_version);

		// preprocess source
		TBuiltInResource resource = GetResources();
		std::string preprocessed_source;
		glslang::InitializeProcess();
		if (!shader.preprocess(&resource,
							   esh_target_version,
							   EProfile::ENoProfile,
							   false,
							   false,
							   esh_messages,
							   &preprocessed_source,
							   includer))
		{
			THROW(source_name + "\n" +
				  "glslang error(log):\n" + std::string(shader.getInfoLog()) +
				  "glslang error(debug log):\n" + std::string(shader.getInfoDebugLog()));
		}
		const char * prep_source_c_str = preprocessed_source.c_str();
		shader.setStrings(&prep_source_c_str, 1);

		// prase
		if (!shader.parse(&resource,
						  esh_target_version,
						  true,
						  esh_messages))
		{
			THROW(source_name + "\n" +
				  "glslang error(log):\n" + std::string(shader.getInfoLog()) +
				  "glslang error(debug log):\n" + std::string(shader.getInfoDebugLog()));
		}

		glslang::TProgram program;
		program.addShader(&shader);

		if (!program.link(esh_messages) || !program.mapIO())
		{
			THROW("glslang linking shader to program error");
		}

		THROW_ASSERT(sizeof(unsigned int) == sizeof(uint32_t),
					 "did not handle the case where unsigned int != 32 bit");
		std::vector<uint32_t> spirv_result;
		glslang::TIntermediate intermediate = *program.getIntermediate(esh_stage);
		glslang::GlslangToSpv(intermediate, spirv_result);

		*output_source = preprocessed_source;
		*spirv = spirv_result;
	}
};