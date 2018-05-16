//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "GLSLANG/ShaderLang.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sstream>
#include <vector>

// define locally to avoid excess clobbering by gl header #defines.
// gl2.h
static const int GL_FRAGMENT_SHADER = 0x8B30;
static const int GL_VERTEX_SHADER = 0x8B31;
// gl2ext.h
static const int GL_GEOMETRY_SHADER_EXT = 0x8DD9;
// gl31.h
static const int GL_COMPUTE_SHADER = 0x91B9;

//
// Return codes from main.
//
enum TFailCode
{
    ESuccess = 0,
    EFailUsage,
    EFailCompile,
    EFailCompilerCreate,
};

struct CompileOptions {
    int32_t use_precision_emulation; // -p

    // Extension enabled unless set to 0.
    int32_t GL_OES_EGL_image_external; // -x=i
    int32_t GL_OES_EGL_standard_derivatives; // -x=d
    int32_t ARB_texture_rectangle; // -x=r
    int32_t EXT_frag_depth; // -x=g
    int32_t EXT_shader_texture_lod; // -x=l
    int32_t EXT_shader_framebuffer_fetch; // -x=f
    int32_t NV_shader_framebuffer_fetch; // -x=n
    int32_t ARM_shader_framebuffer_fetch; // -x=a
    int32_t OVR_multiview; // -x=m
    int32_t YUV_target; // -x=y

    // Set to number desired; disabled if set to 0.
    int32_t EXT_blend_func_extended; // -x=b[NUM]
    int32_t EXT_draw_buffers; // -x=w[NUM]
};

enum class InputType : int32_t {
    GLES = 0,
    WEBGL = 1,
};

struct InputOptions {
    InputType input_type;
    int32_t version_number;
};

enum class OutputType : int32_t {
    GLES = 0,
    GLSL = 1,
    HLSL = 2
};

struct OutputOptions {
    OutputType output_type;
    int32_t version_number;
};

static void usage();
static void LogMsg(const char *msg, const char *name, const int num, const char *logName);

static bool ParseGLSLOutputVersion(const std::string &, ShShaderOutput *outResult);
static bool ParseIntValue(const std::string &, int emptyDefault, int *outValue);

std::string logString;

//
// Set up the per compile resources
//
void GenerateResources(ShBuiltInResources *resources)
{
    sh::InitBuiltInResources(resources);

    resources->MaxVertexAttribs = 8;
    resources->MaxVertexUniformVectors = 128;
    resources->MaxVaryingVectors = 8;
    resources->MaxVertexTextureImageUnits = 0;
    resources->MaxCombinedTextureImageUnits = 8;
    resources->MaxTextureImageUnits = 8;
    resources->MaxFragmentUniformVectors = 16;
    resources->MaxDrawBuffers = 1;
    resources->MaxDualSourceDrawBuffers     = 1;

    resources->OES_standard_derivatives = 0;
    resources->OES_EGL_image_external = 0;
    resources->EXT_geometry_shader      = 1;
}

int InternalValidate(const char *shaderSrc, const sh::GLenum shaderType, int argc, const char *argv[])
{

    TFailCode failCode = ESuccess;

    ShCompileOptions compileOptions = 0;
    int numCompiles = 0;
    ShHandle vertexCompiler = 0;
    ShHandle fragmentCompiler = 0;
    ShHandle computeCompiler  = 0;
    ShHandle geometryCompiler       = 0;
    ShShaderSpec spec = SH_GLES2_SPEC;
    ShShaderOutput output = SH_ESSL_OUTPUT;

    sh::Initialize();

    ShBuiltInResources resources;
    GenerateResources(&resources);

    if (strlen(shaderSrc) == 0) {
        failCode = EFailUsage;
    }

    argc--;
    argv++;
    for (; (argc >= 1) && (failCode == ESuccess); argc--, argv++)
    {
        if (argv[0][0] == '-')
        {
            switch (argv[0][1])
            {
              case 'o': compileOptions |= SH_OBJECT_CODE; break;
              case 'p': resources.WEBGL_debug_shader_precision = 1; break;
              case 's':
                if (argv[0][2] == '=')
                {
                    switch (argv[0][3])
                    {
                        case 'e':
                            if (argv[0][4] == '3')
                            {
                                if (argv[0][5] == '1')
                                {
                                    spec = SH_GLES3_1_SPEC;
                                }
                                else
                                {
                                    spec = SH_GLES3_SPEC;
                                }
                            }
                            else
                            {
                                spec = SH_GLES2_SPEC;
                            }
                            break;
                        case 'w':
                            if (argv[0][4] == '3')
                            {
                                spec = SH_WEBGL3_SPEC;
                            }
                            else if (argv[0][4] == '2')
                            {
                                spec = SH_WEBGL2_SPEC;
                            }
                            else if (argv[0][4] == 'n')
                            {
                                spec = SH_WEBGL_SPEC;
                            }
                            else
                            {
                                spec = SH_WEBGL_SPEC;
                                resources.FragmentPrecisionHigh = 1;
                            }
                            break;
                        default:
                            failCode = EFailUsage;
                    }
                }
                else
                {
                    failCode = EFailUsage;
                }
                break;
              case 'b':
                if (argv[0][2] == '=')
                {
                    switch (argv[0][3])
                    {
                        case 'e':
                            output = SH_ESSL_OUTPUT;
                            compileOptions |= SH_INITIALIZE_UNINITIALIZED_LOCALS;
                            break;
                        case 'g':
                            if (!ParseGLSLOutputVersion(&argv[0][sizeof("-b=g") - 1], &output))
                            {
                                failCode = EFailUsage;
                            }
                            compileOptions |= SH_INITIALIZE_UNINITIALIZED_LOCALS;
                            break;
                        case 'h':
                            if (argv[0][4] == '1' && argv[0][5] == '1')
                            {
                                output = SH_HLSL_4_1_OUTPUT;
                            }
                            else
                            {
                                output = SH_HLSL_3_0_OUTPUT;
                            }
                            break;
                        default:
                            failCode = EFailUsage;
                    }
                }
                else
                {
                    failCode = EFailUsage;
                }
                break;
              case 'x':
                if (argv[0][2] == '=')
                {
                    // clang-format off
                    switch (argv[0][3])
                    {
                      case 'i': resources.OES_EGL_image_external = 1; break;
                      case 'd': resources.OES_standard_derivatives = 1; break;
                      case 'r': resources.ARB_texture_rectangle = 1; break;
                      case 'b':
                          if (ParseIntValue(&argv[0][sizeof("-x=b") - 1], 1,
                                            &resources.MaxDualSourceDrawBuffers))
                          {
                              resources.EXT_blend_func_extended = 1;
                          }
                          else
                          {
                              failCode = EFailUsage;
                          }
                          break;
                      case 'w':
                          if (ParseIntValue(&argv[0][sizeof("-x=w") - 1], 1,
                                            &resources.MaxDrawBuffers))
                          {
                              resources.EXT_draw_buffers = 1;
                          }
                          else
                          {
                              failCode = EFailUsage;
                          }
                          break;
                      case 'g': resources.EXT_frag_depth = 1; break;
                      case 'l': resources.EXT_shader_texture_lod = 1; break;
                      case 'f': resources.EXT_shader_framebuffer_fetch = 1; break;
                      case 'n': resources.NV_shader_framebuffer_fetch = 1; break;
                      case 'a': resources.ARM_shader_framebuffer_fetch = 1; break;
                      case 'm':
                          resources.OVR_multiview = 1;
                          compileOptions |= SH_INITIALIZE_BUILTINS_FOR_INSTANCED_MULTIVIEW;
                          compileOptions |= SH_SELECT_VIEW_IN_NV_GLSL_VERTEX_SHADER;
                          break;
                      case 'y': resources.EXT_YUV_target = 1; break;
                      default: failCode = EFailUsage;
                    }
                    // clang-format on
                }
                else
                {
                    failCode = EFailUsage;
                }
                break;
              default: failCode = EFailUsage;
            }
        }
    }

    if (spec != SH_GLES2_SPEC && spec != SH_WEBGL_SPEC)
    {
        resources.MaxDrawBuffers = 8;
        resources.MaxVertexTextureImageUnits = 16;
        resources.MaxTextureImageUnits       = 16;
    }
    ShHandle compiler = 0;
    switch (shaderType)
    {
        case GL_VERTEX_SHADER:
        if (vertexCompiler == 0)
        {
            vertexCompiler =
                sh::ConstructCompiler(GL_VERTEX_SHADER, spec, output, &resources);
        }
        compiler = vertexCompiler;
        break;
        case GL_FRAGMENT_SHADER:
        if (fragmentCompiler == 0)
        {
            fragmentCompiler =
                sh::ConstructCompiler(GL_FRAGMENT_SHADER, spec, output, &resources);
        }
        compiler = fragmentCompiler;
        break;
        case GL_COMPUTE_SHADER:
            if (computeCompiler == 0)
            {
                computeCompiler =
                    sh::ConstructCompiler(GL_COMPUTE_SHADER, spec, output, &resources);
            }
            compiler = computeCompiler;
            break;
        case GL_GEOMETRY_SHADER_EXT:
            if (geometryCompiler == 0)
            {
                geometryCompiler =
                    sh::ConstructCompiler(GL_GEOMETRY_SHADER_EXT, spec, output, &resources);
            }
            compiler = geometryCompiler;
            break;
        default: break;
    }

    if (failCode == ESuccess) {
        if (compiler)
        {
            bool compiled = sh::Compile(compiler, &shaderSrc, 1, compileOptions);

            LogMsg("BEGIN", "COMPILER", numCompiles, "INFO LOG");
            std::string log = sh::GetInfoLog(compiler);
            logString += log + "\n";
            LogMsg("END", "COMPILER", numCompiles, "INFO LOG");
            logString += "\n\n";

            if (compiled && (compileOptions & SH_OBJECT_CODE))
            {
                LogMsg("BEGIN", "COMPILER", numCompiles, "OBJ CODE");
                std::string code = sh::GetObjectCode(compiler);
                logString += code + "\n";
                LogMsg("END", "COMPILER", numCompiles, "OBJ CODE");
                logString += "\n\n";
            }
            if (!compiled)
                failCode = EFailCompile;
            ++numCompiles;
        }
        else
        {
            failCode = EFailCompilerCreate;
        }
    }

    if ((vertexCompiler == 0) && (fragmentCompiler == 0) && (computeCompiler == 0) &&
        (geometryCompiler == 0))
        failCode = EFailUsage;
    if (failCode == EFailUsage)
        usage();

    if (vertexCompiler)
        sh::Destruct(vertexCompiler);
    if (fragmentCompiler)
        sh::Destruct(fragmentCompiler);
    if (computeCompiler)
        sh::Destruct(computeCompiler);
    if (geometryCompiler)
        sh::Destruct(geometryCompiler);

    sh::Finalize();

    return failCode;
}

// Prevent name mangling for easy emscripten linking.
extern "C" {
    int ValidateShader(const char *shaderSrc, const sh::GLenum shaderType, const char *args, char **printLog) {
        // super basic split on spaces
        std::stringstream ss(args);
        std::string item;
        std::vector<std::string> splitArgs = {"./shader-validator.cpp"};
        char delim = ' ';
        while (std::getline(ss, item, delim)) {
            splitArgs.push_back(std::move(item));
        }

        std::vector<const char*> argv;
        for (const auto& arg : splitArgs) {
            argv.push_back(arg.c_str());
        }
        argv.push_back(nullptr);

        int ret = InternalValidate(shaderSrc, shaderType, argv.size() - 1, argv.data());

        // print accumulated log
        char *p = (char*)malloc(sizeof(char) * (logString.size() + 1));
        strcpy(p, logString.c_str());
        logString.clear();
        *printLog = p;

        return ret;
    }
}

//
//   print usage to stdout
//
void usage()
{
    // clang-format off
    logString +=
        "Usage: translate [-o -l -p -b=e -b=g -b=h9 -x=i -x=d] file1 file2 ...\n"
        "Where: filename : filename ending in .frag or .vert\n"
        "       -o       : print translated code\n"
        "       -p       : use precision emulation\n"
        "       -s=e2    : use GLES2 spec (this is by default)\n"
        "       -s=e3    : use GLES3 spec\n"
        "       -s=e31   : use GLES31 spec (in development)\n"
        "       -s=w     : use WebGL 1.0 spec\n"
        "       -s=wn    : use WebGL 1.0 spec with no highp support in fragment shaders\n"
        "       -s=w2    : use WebGL 2.0 spec\n"
        "       -b=e     : output GLSL ES code (this is by default)\n"
        "       -b=g     : output GLSL code (compatibility profile)\n"
        "       -b=g[NUM]: output GLSL code (NUM can be 130, 140, 150, 330, 400, 410, 420, 430, "
        "440, 450)\n"
        "       -b=h9    : output HLSL9 code\n"
        "       -b=h11   : output HLSL11 code\n"
        "       -x=i     : enable GL_OES_EGL_image_external\n"
        "       -x=d     : enable GL_OES_EGL_standard_derivatives\n"
        "       -x=r     : enable ARB_texture_rectangle\n"
        "       -x=b[NUM]: enable EXT_blend_func_extended (NUM default 1)\n"
        "       -x=w[NUM]: enable EXT_draw_buffers (NUM default 1)\n"
        "       -x=g     : enable EXT_frag_depth\n"
        "       -x=l     : enable EXT_shader_texture_lod\n"
        "       -x=f     : enable EXT_shader_framebuffer_fetch\n"
        "       -x=n     : enable NV_shader_framebuffer_fetch\n"
        "       -x=a     : enable ARM_shader_framebuffer_fetch\n"
        "       -x=m     : enable OVR_multiview\n"
        "       -x=y     : enable YUV_target\n";
    // clang-format on
}

void LogMsg(const char *msg, const char *name, const int num, const char *logName)
{
    logString += "#### ";
    logString += msg;
    logString += " ";
    logString += name;
    logString += " " + std::to_string(num) + " ";
    logString += logName;
    logString += " ####\n";
}

static bool ParseGLSLOutputVersion(const std::string &num, ShShaderOutput *outResult)
{
    if (num.length() == 0)
    {
        *outResult = SH_GLSL_COMPATIBILITY_OUTPUT;
        return true;
    }
    std::istringstream input(num);
    int value;
    if (!(input >> value && input.eof()))
    {
        return false;
    }

    switch (value)
    {
        case 130:
            *outResult = SH_GLSL_130_OUTPUT;
            return true;
        case 140:
            *outResult = SH_GLSL_140_OUTPUT;
            return true;
        case 150:
            *outResult = SH_GLSL_150_CORE_OUTPUT;
            return true;
        case 330:
            *outResult = SH_GLSL_330_CORE_OUTPUT;
            return true;
        case 400:
            *outResult = SH_GLSL_400_CORE_OUTPUT;
            return true;
        case 410:
            *outResult = SH_GLSL_410_CORE_OUTPUT;
            return true;
        case 420:
            *outResult = SH_GLSL_420_CORE_OUTPUT;
            return true;
        case 430:
            *outResult = SH_GLSL_430_CORE_OUTPUT;
            return true;
        case 440:
            *outResult = SH_GLSL_440_CORE_OUTPUT;
            return true;
        case 450:
            *outResult = SH_GLSL_450_CORE_OUTPUT;
            return true;
        default:
            break;
    }
    return false;
}

static bool ParseIntValue(const std::string &num, int emptyDefault, int *outValue)
{
    if (num.length() == 0)
    {
        *outValue = emptyDefault;
        return true;
    }

    std::istringstream input(num);
    int value;
    if (!(input >> value && input.eof()))
    {
        return false;
    }
    *outValue = value;
    return true;
}
