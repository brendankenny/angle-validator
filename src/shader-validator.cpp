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

int InternalValidate(const char *shaderSrc, const sh::GLenum shaderType, const InputOptions &inputOpts, const OutputOptions &outputOpts, const CompileOptions &compileOpts)
{

    TFailCode failCode = ESuccess;

    ShCompileOptions shCompileOptions = 0;
    ShShaderSpec spec = SH_GLES2_SPEC;
    // TODO(bckenny): old default case doesn't set SH_INITIALIZE_UNINITIALIZED_LOCALS. Why not?
    ShShaderOutput output = SH_ESSL_OUTPUT;

    // TODO(bckenny): need these at first in this function?
    sh::Initialize();
    ShBuiltInResources resources;
    GenerateResources(&resources);

    // Always print code (old -o option)
    shCompileOptions |= SH_OBJECT_CODE;

    // TODO(bckenny): switch instead
    // Input spec
    if (inputOpts.input_type == InputType::GLES) { // -s=e
        if (inputOpts.version_number == 31) {
            spec = SH_GLES3_1_SPEC;
        } else if (inputOpts.version_number == 3) {
            spec = SH_GLES3_SPEC;
        } else {
            spec = SH_GLES2_SPEC;
        }
    } else if (inputOpts.input_type == InputType::WEBGL) { // -s=w
        // TODO(bckenny): WEBGL 3??
        if (inputOpts.version_number == 3) {
            spec = SH_WEBGL3_SPEC;
        } else if (inputOpts.version_number == 2) {
            spec = SH_WEBGL2_SPEC;
        } else {
            // TODO(bckenny): support for -s=wn?
            spec = SH_WEBGL_SPEC;
            resources.FragmentPrecisionHigh = 1;
        }
    } else {
        failCode = EFailUsage;
    }

    // Output type
    if (outputOpts.output_type == OutputType::GLES) { // -b=e
        output = SH_ESSL_OUTPUT;
        shCompileOptions |= SH_INITIALIZE_UNINITIALIZED_LOCALS;
    } else if (outputOpts.output_type == OutputType::GLSL) { // -b=g[NUM]
        switch (outputOpts.version_number)
        {
            case 130:
                output = SH_GLSL_130_OUTPUT;
                break;
            case 140:
                output = SH_GLSL_140_OUTPUT;
                break;
            case 150:
                output = SH_GLSL_150_CORE_OUTPUT;
                break;
            case 330:
                output = SH_GLSL_330_CORE_OUTPUT;
                break;
            case 400:
                output = SH_GLSL_400_CORE_OUTPUT;
                break;
            case 410:
                output = SH_GLSL_410_CORE_OUTPUT;
                break;
            case 420:
                output = SH_GLSL_420_CORE_OUTPUT;
                break;
            case 430:
                output = SH_GLSL_430_CORE_OUTPUT;
                break;
            case 440:
                output = SH_GLSL_440_CORE_OUTPUT;
                break;
            case 450:
                output = SH_GLSL_450_CORE_OUTPUT;
                break;
            default:
                output = SH_GLSL_COMPATIBILITY_OUTPUT;
                break;
        }
        shCompileOptions |= SH_INITIALIZE_UNINITIALIZED_LOCALS;
    } else if (outputOpts.output_type == OutputType::HLSL) { // -b=h[9|11]
        if (outputOpts.version_number == 11) {
            output = SH_HLSL_4_1_OUTPUT;
        } else {
            output = SH_HLSL_3_0_OUTPUT;
        }
    } else {
        failCode = EFailUsage;
    }

    // Compile options
    if (compileOpts.use_precision_emulation) {
        resources.WEBGL_debug_shader_precision = 1;
    }

    // TODO(bckenny): verify actual defaults false/0?
    if (compileOpts.GL_OES_EGL_image_external) {
        resources.OES_EGL_image_external = 1;
    }
    if (compileOpts.GL_OES_EGL_standard_derivatives) {
        resources.OES_standard_derivatives = 1;
    }
    if (compileOpts.ARB_texture_rectangle) {
        resources.ARB_texture_rectangle = 1;
    }
    if (compileOpts.EXT_frag_depth) {
        resources.EXT_frag_depth = 1;
    }
    if (compileOpts.EXT_shader_texture_lod) {
        resources.EXT_shader_texture_lod = 1;
    }
    if (compileOpts.EXT_shader_framebuffer_fetch) {
        resources.EXT_shader_framebuffer_fetch = 1;
    }
    if (compileOpts.NV_shader_framebuffer_fetch) {
        resources.NV_shader_framebuffer_fetch = 1;
    }
    if (compileOpts.ARM_shader_framebuffer_fetch) {
        resources.ARM_shader_framebuffer_fetch = 1;
    }
    if (compileOpts.OVR_multiview) {
        resources.OVR_multiview = 1;
        shCompileOptions |= SH_INITIALIZE_BUILTINS_FOR_INSTANCED_MULTIVIEW;
        shCompileOptions |= SH_SELECT_VIEW_IN_NV_GLSL_VERTEX_SHADER;
    }
    if (compileOpts.YUV_target) {
        resources.EXT_YUV_target = 1;
    }
    // TODO(bckenny): for next two, could maybe rename property to differentiate from resources prop
    if (compileOpts.EXT_blend_func_extended) {
        resources.EXT_blend_func_extended = 1;
        resources.MaxDualSourceDrawBuffers = compileOpts.EXT_blend_func_extended;
    }
    if (compileOpts.EXT_draw_buffers) {
        resources.EXT_draw_buffers = 1;
        resources.MaxDrawBuffers = compileOpts.EXT_draw_buffers;
    }

    if (spec != SH_GLES2_SPEC && spec != SH_WEBGL_SPEC)
    {
        // TODO(bckenny): meant to overwrite EXT_draw_buffers option?
        resources.MaxDrawBuffers = 8;
        resources.MaxVertexTextureImageUnits = 16;
        resources.MaxTextureImageUnits       = 16;
    }

    ShHandle compiler = 0;
    switch (shaderType)
    {
        case GL_VERTEX_SHADER:
            compiler = sh::ConstructCompiler(GL_VERTEX_SHADER, spec, output, &resources);
            break;
        case GL_FRAGMENT_SHADER:
            compiler = sh::ConstructCompiler(GL_FRAGMENT_SHADER, spec, output, &resources);
            break;
        case GL_COMPUTE_SHADER:
            compiler = sh::ConstructCompiler(GL_COMPUTE_SHADER, spec, output, &resources);
            break;
        case GL_GEOMETRY_SHADER_EXT:
            compiler = sh::ConstructCompiler(GL_GEOMETRY_SHADER_EXT, spec, output, &resources);
            break;
        default: break;
    }

    if (failCode == ESuccess) {
        if (compiler)
        {
            bool compiled = sh::Compile(compiler, &shaderSrc, 1, shCompileOptions);

            LogMsg("BEGIN", "COMPILER", 0, "INFO LOG");
            std::string log = sh::GetInfoLog(compiler);
            logString += log + "\n";
            LogMsg("END", "COMPILER", 0, "INFO LOG");
            logString += "\n\n";

            // TODO(bckenny): always get object code.
            if (compiled && (shCompileOptions & SH_OBJECT_CODE))
            {
                LogMsg("BEGIN", "COMPILER", 0, "OBJ CODE");
                std::string code = sh::GetObjectCode(compiler);
                logString += code + "\n";
                LogMsg("END", "COMPILER", 0, "OBJ CODE");
                logString += "\n\n";
            }
            if (!compiled)
                failCode = EFailCompile;
        }
        else
        {
            failCode = EFailCompilerCreate;
        }
    }

    if (compiler == 0)
        failCode = EFailUsage;
    if (failCode == EFailUsage)
    {
        // TODO(bckenny): handle signal of invalid input
        usage();
    }
        

    if (compiler)
        sh::Destruct(compiler);

    sh::Finalize();

    return failCode;
}

// Prevent name mangling for easy emscripten linking.
extern "C" {
    int ValidateShader(const char *shaderSrc, const sh::GLenum shaderType, const InputOptions inputOptions, const OutputOptions outputOptions, const CompileOptions compileOptions, char **printLog) {
        int ret = InternalValidate(shaderSrc, shaderType, inputOptions, outputOptions, compileOptions);

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
