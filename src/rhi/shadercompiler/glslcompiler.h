#pragma once

#include "common/logger.h"
#include "graphicsapi/enums.h"
#include "graphicsapi/shadersrc.h"

#include <SPIRV/GlslangToSpv.h>
#include <StandAlone/DirStackFileIncluder.h>
#include <glslang/Public/ShaderLang.h>

struct GlslCompiler
{
    static const EShMessages EshMessageModeFlags = EShMessages(EShMessages::EShMsgSpvRules | EShMessages::EShMsgVulkanRules |
                                                               EShMessages::EShMsgDefault | EShMessages::EShMsgDebugInfo);

    static inline const std::map<ShaderStageEnum, EShLanguage> EshLanguageMapper = {
        { ShaderStageEnum::Vertex, EShLanguage::EShLangVertex },
        { ShaderStageEnum::Fragment, EShLanguage::EShLangFragment },
        { ShaderStageEnum::Geometry, EShLanguage::EShLangGeometry },
        { ShaderStageEnum::Compute, EShLanguage::EShLangCompute },
        { ShaderStageEnum::RayGen, EShLanguage::EShLangRayGen },
        { ShaderStageEnum::AnyHit, EShLanguage::EShLangAnyHit },
        { ShaderStageEnum::ClosestHit, EShLanguage::EShLangClosestHit },
        { ShaderStageEnum::Miss, EShLanguage::EShLangMiss }
    };

    TBuiltInResource m_glslang_builtin_resource = init_glslang_builtin_resource();
    uint32_t         m_vulkan_made_version;

    GlslCompiler(const uint32_t vulkan_made_version = glslang::EshTargetClientVersion::EShTargetVulkan_1_0)
    : m_vulkan_made_version(vulkan_made_version)
    {
    }

    std::vector<uint32_t>
    compile_as_spirv(const std::string & shader_name, const std::string & shader_string, const std::string & entry, const std::filesystem::path & path, const ShaderStageEnum shader_stage)
    {
        // initialize glslang process
        glslang::InitializeProcess();

        // language setting
        const auto esh_target_version     = glslang::EshTargetClientVersion(m_vulkan_made_version);
        const auto esh_target_spv_version = glslang::EShTargetSpv_1_5;
        if (EshLanguageMapper.count(shader_stage) == 0)
        {
            Logger::Error<true>(__FUNCTION__ " encountered unknown shader stage");
        }
        const EShLanguage esh_stage = EshLanguageMapper.at(shader_stage);

        // declare includer
        DirStackFileIncluder includer;
        includer.pushExternalLocalDirectory((std::filesystem::current_path() / "shaders").string());
        if (path != "")
        {
            includer.pushExternalLocalDirectory(path.string());
        }


        // shader
        glslang::TShader shader(esh_stage);
        const char *     shader_str_c_str = shader_string.c_str();
        shader.setStrings(&shader_str_c_str, 1);
        shader.setEntryPoint(entry.c_str());
        shader.setSourceEntryPoint(entry.c_str());
        shader.setEnvInput(glslang::EShSourceGlsl, esh_stage, glslang::EShClientVulkan, m_vulkan_made_version);
        shader.setEnvClient(glslang::EShClientVulkan, esh_target_version);
        shader.setEnvTarget(glslang::EshTargetSpv, esh_target_spv_version);

        // preprocess source
        {
            std::string preprocessed_source;
            if (!shader.preprocess(&m_glslang_builtin_resource, esh_target_version, EProfile::ENoProfile, false, false, EshMessageModeFlags, &preprocessed_source, includer))
            {
                Logger::Error<true>(__FUNCTION__ " could not preprocess the source : " + shader_name + "\n" +
                                    "glslang preprocessing error(log):\n" + std::string(shader.getInfoLog()) +
                                    "glslang error(debug log):\n" + std::string(shader.getInfoDebugLog()));
            }
            const char * prep_source_c_str = preprocessed_source.c_str();
            shader.setStrings(&prep_source_c_str, 1);
        }

        // parse
        if (!shader.parse(&m_glslang_builtin_resource, esh_target_version, true, EshMessageModeFlags))
        {
            Logger::Error<true>(__FUNCTION__ " could not parse of the source : " + shader_name + "\n" +
                                "glslang error(log):\n" + std::string(shader.getInfoLog()) +
                                "glslang error(debug log):\n" + std::string(shader.getInfoDebugLog()));
        }

        // compile to spir-v
        std::vector<uint32_t> code;
        glslang::TProgram     program;
        program.addShader(&shader);
        if (!program.link(EshMessageModeFlags) || !program.mapIO())
        {
            Logger::Error<true>(__FUNCTION__ " could not link the source : " + shader_name + "\n" +
                                "glslang linking shader to program error");
        }
        glslang::TIntermediate * intermediate = program.getIntermediate(esh_stage);
        glslang::GlslangToSpv(*intermediate, code);

        // return the code
        return code;
    }

    std::vector<uint32_t>
    compile_as_spirv(const ShaderSrc & shader_src)
    {
        return compile_as_spirv(shader_src.m_name, shader_src.source(), shader_src.m_entry, shader_src.m_file_path, shader_src.m_shader_stage);
    }

    TBuiltInResource
    init_glslang_builtin_resource() const
    {
        TBuiltInResource resources                            = {};
        resources.maxLights                                   = 32;
        resources.maxClipPlanes                               = 6;
        resources.maxTextureUnits                             = 32;
        resources.maxTextureCoords                            = 32;
        resources.maxVertexAttribs                            = 64;
        resources.maxVertexUniformComponents                  = 4096;
        resources.maxVaryingFloats                            = 64;
        resources.maxVertexTextureImageUnits                  = 32;
        resources.maxCombinedTextureImageUnits                = 80;
        resources.maxTextureImageUnits                        = 32;
        resources.maxFragmentUniformComponents                = 4096;
        resources.maxDrawBuffers                              = 32;
        resources.maxVertexUniformVectors                     = 128;
        resources.maxVaryingVectors                           = 8;
        resources.maxFragmentUniformVectors                   = 16;
        resources.maxVertexOutputVectors                      = 16;
        resources.maxFragmentInputVectors                     = 15;
        resources.minProgramTexelOffset                       = -8;
        resources.maxProgramTexelOffset                       = 7;
        resources.maxClipDistances                            = 8;
        resources.maxComputeWorkGroupCountX                   = 65535;
        resources.maxComputeWorkGroupCountY                   = 65535;
        resources.maxComputeWorkGroupCountZ                   = 65535;
        resources.maxComputeWorkGroupSizeX                    = 1024;
        resources.maxComputeWorkGroupSizeY                    = 1024;
        resources.maxComputeWorkGroupSizeZ                    = 64;
        resources.maxComputeUniformComponents                 = 1024;
        resources.maxComputeTextureImageUnits                 = 16;
        resources.maxComputeImageUniforms                     = 8;
        resources.maxComputeAtomicCounters                    = 8;
        resources.maxComputeAtomicCounterBuffers              = 1;
        resources.maxVaryingComponents                        = 60;
        resources.maxVertexOutputComponents                   = 64;
        resources.maxGeometryInputComponents                  = 64;
        resources.maxGeometryOutputComponents                 = 128;
        resources.maxFragmentInputComponents                  = 128;
        resources.maxImageUnits                               = 8;
        resources.maxCombinedImageUnitsAndFragmentOutputs     = 8;
        resources.maxCombinedShaderOutputResources            = 8;
        resources.maxImageSamples                             = 0;
        resources.maxVertexImageUniforms                      = 0;
        resources.maxTessControlImageUniforms                 = 0;
        resources.maxTessEvaluationImageUniforms              = 0;
        resources.maxGeometryImageUniforms                    = 0;
        resources.maxFragmentImageUniforms                    = 8;
        resources.maxCombinedImageUniforms                    = 8;
        resources.maxGeometryTextureImageUnits                = 16;
        resources.maxGeometryOutputVertices                   = 256;
        resources.maxGeometryTotalOutputComponents            = 1024;
        resources.maxGeometryUniformComponents                = 1024;
        resources.maxGeometryVaryingComponents                = 64;
        resources.maxTessControlInputComponents               = 128;
        resources.maxTessControlOutputComponents              = 128;
        resources.maxTessControlTextureImageUnits             = 16;
        resources.maxTessControlUniformComponents             = 1024;
        resources.maxTessControlTotalOutputComponents         = 4096;
        resources.maxTessEvaluationInputComponents            = 128;
        resources.maxTessEvaluationOutputComponents           = 128;
        resources.maxTessEvaluationTextureImageUnits          = 16;
        resources.maxTessEvaluationUniformComponents          = 1024;
        resources.maxTessPatchComponents                      = 120;
        resources.maxPatchVertices                            = 32;
        resources.maxTessGenLevel                             = 64;
        resources.maxViewports                                = 16;
        resources.maxVertexAtomicCounters                     = 0;
        resources.maxTessControlAtomicCounters                = 0;
        resources.maxTessEvaluationAtomicCounters             = 0;
        resources.maxGeometryAtomicCounters                   = 0;
        resources.maxFragmentAtomicCounters                   = 8;
        resources.maxCombinedAtomicCounters                   = 8;
        resources.maxAtomicCounterBindings                    = 1;
        resources.maxVertexAtomicCounterBuffers               = 0;
        resources.maxTessControlAtomicCounterBuffers          = 0;
        resources.maxTessEvaluationAtomicCounterBuffers       = 0;
        resources.maxGeometryAtomicCounterBuffers             = 0;
        resources.maxFragmentAtomicCounterBuffers             = 1;
        resources.maxCombinedAtomicCounterBuffers             = 1;
        resources.maxAtomicCounterBufferSize                  = 16384;
        resources.maxTransformFeedbackBuffers                 = 4;
        resources.maxTransformFeedbackInterleavedComponents   = 64;
        resources.maxCullDistances                            = 8;
        resources.maxCombinedClipAndCullDistances             = 8;
        resources.maxSamples                                  = 4;
        resources.limits.nonInductiveForLoops                 = true;
        resources.limits.whileLoops                           = true;
        resources.limits.doWhileLoops                         = true;
        resources.limits.generalUniformIndexing               = true;
        resources.limits.generalAttributeMatrixVectorIndexing = true;
        resources.limits.generalVaryingIndexing               = true;
        resources.limits.generalSamplerIndexing               = true;
        resources.limits.generalVariableIndexing              = true;
        resources.limits.generalConstantMatrixVectorIndexing  = true;
        return resources;
    }
};
