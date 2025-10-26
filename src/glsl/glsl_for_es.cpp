#include "glsl_for_es.h"

#include <glslang/Public/ShaderLang.h>
#include <glslang/Include/Types.h>
#include <glslang/Public/ShaderLang.h>
#include <spirv_cross/spirv_cross_c.h>
#include <iostream>
#include "../gl/gl4es.h"
#include <fstream>
#include "../gl/logs.h"
#include "glslang/SPIRV/GlslangToSpv.h"
#include <string>
#include <regex>
#include <strstream>

#define DBG(d)

#define GL_COMPUTE_SHADER 0x91B9

typedef std::vector<uint32_t> Spirv;

static TBuiltInResource InitResources()
{
    TBuiltInResource Resources;

    Resources.maxLights                                 = 32;
    Resources.maxClipPlanes                             = 6;
    Resources.maxTextureUnits                           = 32;
    Resources.maxTextureCoords                          = 32;
    Resources.maxVertexAttribs                          = 64;
    Resources.maxVertexUniformComponents                = 4096;
    Resources.maxVaryingFloats                          = 64;
    Resources.maxVertexTextureImageUnits                = 32;
    Resources.maxCombinedTextureImageUnits              = 80;
    Resources.maxTextureImageUnits                      = 32;
    Resources.maxFragmentUniformComponents              = 4096;
    Resources.maxDrawBuffers                            = 32;
    Resources.maxVertexUniformVectors                   = 128;
    Resources.maxVaryingVectors                         = 8;
    Resources.maxFragmentUniformVectors                 = 16;
    Resources.maxVertexOutputVectors                    = 16;
    Resources.maxFragmentInputVectors                   = 15;
    Resources.minProgramTexelOffset                     = -8;
    Resources.maxProgramTexelOffset                     = 7;
    Resources.maxClipDistances                          = 8;
    Resources.maxComputeWorkGroupCountX                 = 65535;
    Resources.maxComputeWorkGroupCountY                 = 65535;
    Resources.maxComputeWorkGroupCountZ                 = 65535;
    Resources.maxComputeWorkGroupSizeX                  = 1024;
    Resources.maxComputeWorkGroupSizeY                  = 1024;
    Resources.maxComputeWorkGroupSizeZ                  = 64;
    Resources.maxComputeUniformComponents               = 1024;
    Resources.maxComputeTextureImageUnits               = 16;
    Resources.maxComputeImageUniforms                   = 8;
    Resources.maxComputeAtomicCounters                  = 8;
    Resources.maxComputeAtomicCounterBuffers            = 1;
    Resources.maxVaryingComponents                      = 60;
    Resources.maxVertexOutputComponents                 = 64;
    Resources.maxGeometryInputComponents                = 64;
    Resources.maxGeometryOutputComponents               = 128;
    Resources.maxFragmentInputComponents                = 128;
    Resources.maxImageUnits                             = 8;
    Resources.maxCombinedImageUnitsAndFragmentOutputs   = 8;
    Resources.maxCombinedShaderOutputResources          = 8;
    Resources.maxImageSamples                           = 0;
    Resources.maxVertexImageUniforms                    = 0;
    Resources.maxTessControlImageUniforms               = 0;
    Resources.maxTessEvaluationImageUniforms            = 0;
    Resources.maxGeometryImageUniforms                  = 0;
    Resources.maxFragmentImageUniforms                  = 8;
    Resources.maxCombinedImageUniforms                  = 8;
    Resources.maxGeometryTextureImageUnits              = 16;
    Resources.maxGeometryOutputVertices                 = 256;
    Resources.maxGeometryTotalOutputComponents          = 1024;
    Resources.maxGeometryUniformComponents              = 1024;
    Resources.maxGeometryVaryingComponents              = 64;
    Resources.maxTessControlInputComponents             = 128;
    Resources.maxTessControlOutputComponents            = 128;
    Resources.maxTessControlTextureImageUnits           = 16;
    Resources.maxTessControlUniformComponents           = 1024;
    Resources.maxTessControlTotalOutputComponents       = 4096;
    Resources.maxTessEvaluationInputComponents          = 128;
    Resources.maxTessEvaluationOutputComponents         = 128;
    Resources.maxTessEvaluationTextureImageUnits        = 16;
    Resources.maxTessEvaluationUniformComponents        = 1024;
    Resources.maxTessPatchComponents                    = 120;
    Resources.maxPatchVertices                          = 32;
    Resources.maxTessGenLevel                           = 64;
    Resources.maxViewports                              = 16;
    Resources.maxVertexAtomicCounters                   = 0;
    Resources.maxTessControlAtomicCounters              = 0;
    Resources.maxTessEvaluationAtomicCounters           = 0;
    Resources.maxGeometryAtomicCounters                 = 0;
    Resources.maxFragmentAtomicCounters                 = 8;
    Resources.maxCombinedAtomicCounters                 = 8;
    Resources.maxAtomicCounterBindings                  = 1;
    Resources.maxVertexAtomicCounterBuffers             = 0;
    Resources.maxTessControlAtomicCounterBuffers        = 0;
    Resources.maxTessEvaluationAtomicCounterBuffers     = 0;
    Resources.maxGeometryAtomicCounterBuffers           = 0;
    Resources.maxFragmentAtomicCounterBuffers           = 1;
    Resources.maxCombinedAtomicCounterBuffers           = 1;
    Resources.maxAtomicCounterBufferSize                = 16384;
    Resources.maxTransformFeedbackBuffers               = 4;
    Resources.maxTransformFeedbackInterleavedComponents = 64;
    Resources.maxCullDistances                          = 8;
    Resources.maxCombinedClipAndCullDistances           = 8;
    Resources.maxSamples                                = 4;
    Resources.maxMeshOutputVerticesNV                   = 256;
    Resources.maxMeshOutputPrimitivesNV                 = 512;
    Resources.maxMeshWorkGroupSizeX_NV                  = 32;
    Resources.maxMeshWorkGroupSizeY_NV                  = 1;
    Resources.maxMeshWorkGroupSizeZ_NV                  = 1;
    Resources.maxTaskWorkGroupSizeX_NV                  = 32;
    Resources.maxTaskWorkGroupSizeY_NV                  = 1;
    Resources.maxTaskWorkGroupSizeZ_NV                  = 1;
    Resources.maxMeshViewCountNV                        = 4;

    Resources.limits.nonInductiveForLoops                 = 1;
    Resources.limits.whileLoops                           = 1;
    Resources.limits.doWhileLoops                         = 1;
    Resources.limits.generalUniformIndexing               = 1;
    Resources.limits.generalAttributeMatrixVectorIndexing = 1;
    Resources.limits.generalVaryingIndexing               = 1;
    Resources.limits.generalSamplerIndexing               = 1;
    Resources.limits.generalVariableIndexing              = 1;
    Resources.limits.generalConstantMatrixVectorIndexing  = 1;

    return Resources;
}

int getGLSLVersion(const char* glsl_code) {
    std::string code(glsl_code);
    std::regex version_pattern(R"(#version\s+(\d{3}))");
    std::smatch match;
    if (std::regex_search(code, match, version_pattern)) {
        return std::stoi(match[1].str());
    }

    return -1;
}

std::string removeSecondLine(std::string code) {
    size_t firstLineEnd = code.find('\n');
    if (firstLineEnd == std::string::npos) {
        return code;
    }
    size_t secondLineEnd = code.find('\n', firstLineEnd + 1);
    if (secondLineEnd == std::string::npos) {
        return code;
    }
    code.erase(firstLineEnd + 1, secondLineEnd - firstLineEnd);
    return code;
}

char* disable_GL_ARB_derivative_control(char* glslCode) {
    std::string code(glslCode);
    std::string target = "GL_ARB_derivative_control";
    size_t pos = code.find(target);

    if (pos != std::string::npos) {
        size_t ifdefPos = 0;
        while ((ifdefPos = code.find("#ifdef GL_ARB_derivative_control", ifdefPos)) != std::string::npos) {
            code.replace(ifdefPos, 32, "#if 0");
            ifdefPos += 4;
        }

        size_t ifndefPos = 0;
        while ((ifndefPos = code.find("#ifndef GL_ARB_derivative_control", ifndefPos)) != std::string::npos) {
            code.replace(ifndefPos, 33, "#if 1");
            ifndefPos += 4;
        }

        code = removeSecondLine(code);

        char* result = new char[code.length() + 1];
        std::strcpy(result, code.c_str());
        return result;
    }

    char* result = new char[code.length() + 1];
    std::strcpy(result, code.c_str());
    return result;
}

std::string forceSupporter(const std::string& glslCode) {
    bool hasPrecisionFloat = glslCode.find("precision ") != std::string::npos &&
                             glslCode.find("float;") != std::string::npos;
    bool hasPrecisionInt = glslCode.find("precision ") != std::string::npos &&
                           glslCode.find("int;") != std::string::npos;
    if (hasPrecisionFloat && hasPrecisionInt) {
        return glslCode;
    }
    std::string result = glslCode;
    std::string precisionFloat = hasPrecisionFloat ? "" : "precision highp float;\n";
    std::string precisionInt = hasPrecisionInt ? "" : "precision highp int;\n";
    size_t lastExtensionPos = result.rfind("#extension");
    size_t insertionPos = 0;
    if (lastExtensionPos != std::string::npos) {
        size_t nextNewline = result.find('\n', lastExtensionPos);
        if (nextNewline != std::string::npos) {
            insertionPos = nextNewline + 1;
        } else {
            insertionPos = result.length();
        }
    } else {
        size_t firstNewline = result.find('\n');
        if (firstNewline != std::string::npos) {
            insertionPos = firstNewline + 1;
        } else {
            result = precisionFloat + precisionInt + result;
            return result;
        }
    }
    result.insert(insertionPos, precisionFloat + precisionInt);
    return result;
}

std::string removeLayoutBinding(const std::string& glslCode) {
    std::regex bindingRegex(R"(layout\s*\(\s*binding\s*=\s*\d+\s*\)\s*)");
    std::string result = std::regex_replace(glslCode, bindingRegex, "");
    return result;
}

std::string removeLocationBinding(const std::string& glslCode) {
    std::regex locationRegex(R"(layout\s*\(\s*location\s*=\s*\d+\s*\)\s*)");
    std::string result = std::regex_replace(glslCode, locationRegex, "");
    return result;
}

char* removeLineDirective(char* glslCode) {
    char* cursor = glslCode;
    int modifiedCodeIndex = 0;
    size_t maxLength = 1024 * 10;
    char* modifiedGlslCode = (char*)malloc(maxLength * sizeof(char));
    if (!modifiedGlslCode) return NULL;

    while (*cursor) {
        if (strncmp(cursor, "\n#", 2) == 0) {
            modifiedGlslCode[modifiedCodeIndex++] = *cursor++;
            modifiedGlslCode[modifiedCodeIndex++] = *cursor++;
            char* last_cursor = cursor;
            while (cursor[0] != '\n') cursor++;
            char* line_feed_cursor = cursor;
            while (isspace(cursor[0])) cursor--;
            if (cursor[0] == '\\')
            {
                // find line directive, now remove it
                char* slash_cursor = cursor;
                cursor = last_cursor;
                while (cursor < slash_cursor - 1)
                    modifiedGlslCode[modifiedCodeIndex++] = *cursor++;
                modifiedGlslCode[modifiedCodeIndex++] = ' ';
                cursor = line_feed_cursor + 1;
                while (isspace(cursor[0])) cursor++;

                while (true) {
                    char* last_cursor2 = cursor;
                    while (cursor[0] != '\n') cursor++;
                    cursor -= 1;
                    while (isspace(cursor[0])) cursor--;
                    if (cursor[0] == '\\') {
                        char* slash_cursor2 = cursor;
                        cursor = last_cursor2;
                        while (cursor < slash_cursor2)
                            modifiedGlslCode[modifiedCodeIndex++] = *cursor++;
                        while (cursor[0] != '\n') cursor++;
                        cursor++;
                        while (isspace(cursor[0])) cursor++;
                    } else {
                        cursor = last_cursor2;
                        while (cursor[0] != '\n')
                            modifiedGlslCode[modifiedCodeIndex++] = *cursor++;
                        break;
                    }
                }
                cursor++;
            }
            else {
                cursor = last_cursor;
            }
        }
        else {
            modifiedGlslCode[modifiedCodeIndex++] = *cursor++;
        }

        if (modifiedCodeIndex >= maxLength - 1) {
            maxLength *= 2;
            modifiedGlslCode = (char*)realloc(modifiedGlslCode, maxLength);
            if (!modifiedGlslCode) return NULL;
        }
    }

    modifiedGlslCode[modifiedCodeIndex] = '\0';
    return modifiedGlslCode;
}

std::string replaceText(const std::string& input, const std::string& from, const std::string& to) {
    std::string result = input;
    size_t pos = 0;
    while ((pos = result.find(from, pos)) != std::string::npos) {
        result.replace(pos, from.length(), to);
        pos += to.length();
    }
    return result;
}

std::string addPrecisionToSampler2DShadow(const std::string& glslCode) {
    std::string result = glslCode;
    result = replaceText(result, " sampler2DShadow ", " highp sampler2DShadow ");
    result = replaceText(result, " mediump highp ", " mediump ");
    result = replaceText(result, " lowp highp ", " lowp ");
    result = replaceText(result, " highp highp ", " highp ");
    return result;
}

char* GLSLtoGLSLES(char* glsl_code, GLenum glsl_type, uint essl_version) {
    return "";
}
