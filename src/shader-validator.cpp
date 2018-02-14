//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "GLSLANG/ShaderLang.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <regex>
#include <sstream>
#include <vector>
#include "angle_gl.h"

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

const std::string aqFishFrag = R"(/**
 * Copyright 2009 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// https://github.com/WebGLSamples/WebGLSamples.github.io/tree/42019d791baffe5d7b88eadfad404f80553025e8/aquarium

precision mediump float;
uniform vec4 lightColor;
varying vec4 v_position;
varying vec2 v_texCoord;
varying vec3 v_tangent;  // #normalMap
varying vec3 v_binormal;  // #normalMap
varying vec3 v_normal;
varying vec3 v_surfaceToLight;
varying vec3 v_surfaceToView;

uniform vec4 ambient;
uniform sampler2D diffuse;
uniform vec4 specular;
uniform sampler2D normalMap;  // #normalMap
uniform float shininess;
uniform float specularFactor;
// #fogUniforms

vec4 lit(float l ,float h, float m) {
  return vec4(1.0,
              max(l, 0.0),
              (l > 0.0) ? pow(max(0.0, h), m) : 0.0,
              1.0);
}
void main() {
  vec4 diffuseColor = texture2D(diffuse, v_texCoord);
  mat3 tangentToWorld = mat3(v_tangent,  // #normalMap
                             v_binormal,  // #normalMap
                             v_normal);  // #normalMap
  vec4 normalSpec = texture2D(normalMap, v_texCoord.xy);  // #normalMap
  vec3 tangentNormal = normalSpec.xyz - vec3(0.5, 0.5, 0.5);  // #normalMap
  tangentNormal = normalize(tangentNormal + vec3(0, 0, 2));  // #normalMap
  vec3 normal = (tangentToWorld * tangentNormal);  // #normalMap
  normal = normalize(normal);  // #normalMap
  vec3 surfaceToLight = normalize(v_surfaceToLight);
  vec3 surfaceToView = normalize(v_surfaceToView);
  vec3 halfVector = normalize(surfaceToLight + surfaceToView);
  vec4 litR = lit(dot(normal, surfaceToLight),
                    dot(normal, halfVector), shininess);
  vec4 outColor = vec4(
    (lightColor * (diffuseColor * litR.y + diffuseColor * ambient +
                  specular * litR.z * specularFactor * normalSpec.a)).rgb,
      diffuseColor.a);
  // #fogCode
  gl_FragColor = outColor;
}
)";

const std::string multiVert = R"(#version 300 es

/**
 * Copyright (c) 2017 The ANGLE Project Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// https://github.com/google/angle/blob/8f27b05092640bde80e8a1e6bda9f364f7921960/samples/multiview/Multiview.cpp

#extension GL_OVR_multiview : require

layout(num_views = 2) in;
layout(location=0) in vec3 posIn;
layout(location=1) in vec3 normalIn;

uniform mat4 uPerspective;
uniform mat4 uCameraLeftEye;
uniform mat4 uCameraRightEye;
uniform mat4 uTranslation;

out vec3 oNormal;

void main()
{
  vec4 p = uTranslation * vec4(posIn,1.);
  if (gl_ViewID_OVR == 0u) {
    p = uCameraLeftEye * p;
  } else {
    p = uCameraRightEye * p;
  }
  oNormal = normalIn;
  gl_Position = uPerspective * p;
}
)";

static void usage();
static sh::GLenum FindShaderType(const char *fileName);
static bool CompileFile(char *fileName, ShHandle compiler, ShCompileOptions compileOptions);
static void LogMsg(const char *msg, const char *name, const int num, const char *logName);
static void PrintVariable(const std::string &prefix, size_t index, const sh::ShaderVariable &var);
static void PrintActiveVariables(ShHandle compiler);

static bool ParseGLSLOutputVersion(const std::string &, ShShaderOutput *outResult);
static bool ParseIntValue(const std::string &, int emptyDefault, int *outValue);

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

int main(int argc, char *argv[])
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

    argc--;
    argv++;
    for (; (argc >= 1) && (failCode == ESuccess); argc--, argv++)
    {
        if (argv[0][0] == '-')
        {
            switch (argv[0][1])
            {
              case 'i': compileOptions |= SH_INTERMEDIATE_TREE; break;
              case 'o': compileOptions |= SH_OBJECT_CODE; break;
              case 'u': compileOptions |= SH_VARIABLES; break;
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
        else
        {
            if (spec != SH_GLES2_SPEC && spec != SH_WEBGL_SPEC)
            {
                resources.MaxDrawBuffers = 8;
                resources.MaxVertexTextureImageUnits = 16;
                resources.MaxTextureImageUnits       = 16;
            }
            ShHandle compiler = 0;
            switch (FindShaderType(argv[0]))
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
            if (compiler)
            {
                bool compiled = CompileFile(argv[0], compiler, compileOptions);

                LogMsg("BEGIN", "COMPILER", numCompiles, "INFO LOG");
                std::string log = sh::GetInfoLog(compiler);
                puts(log.c_str());
                LogMsg("END", "COMPILER", numCompiles, "INFO LOG");
                printf("\n\n");

                if (compiled && (compileOptions & SH_OBJECT_CODE))
                {
                    LogMsg("BEGIN", "COMPILER", numCompiles, "OBJ CODE");
                    std::string code = sh::GetObjectCode(compiler);
                    puts(code.c_str());
                    LogMsg("END", "COMPILER", numCompiles, "OBJ CODE");
                    printf("\n\n");
                }
                if (compiled && (compileOptions & SH_VARIABLES))
                {
                    LogMsg("BEGIN", "COMPILER", numCompiles, "VARIABLES");
                    PrintActiveVariables(compiler);
                    LogMsg("END", "COMPILER", numCompiles, "VARIABLES");
                    printf("\n\n");
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

//
//   print usage to stdout
//
void usage()
{
    // clang-format off
    printf(
        "Usage: translate [-i -o -u -l -p -b=e -b=g -b=h9 -x=i -x=d] file1 file2 ...\n"
        "Where: filename : filename ending in .frag or .vert\n"
        "       -i       : print intermediate tree\n"
        "       -o       : print translated code\n"
        "       -u       : print active attribs, uniforms, varyings and program outputs\n"
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
        "       -x=y     : enable YUV_target\n");
    // clang-format on
}

//
//   Deduce the shader type from the filename.  Files must end in one of the
//   following extensions:
//
//   .frag*    = fragment shader
//   .vert*    = vertex shader
//
sh::GLenum FindShaderType(const char *fileName)
{

    const char *ext = strrchr(fileName, '.');

    if (ext && strcmp(ext, ".sl") == 0)
        for (; ext > fileName && ext[0] != '.'; ext--);

    ext = strrchr(fileName, '.');
    if (ext)
    {
        if (strncmp(ext, ".frag", 5) == 0)
            return GL_FRAGMENT_SHADER;
        if (strncmp(ext, ".vert", 5) == 0)
            return GL_VERTEX_SHADER;
        if (strncmp(ext, ".comp", 5) == 0)
            return GL_COMPUTE_SHADER;
        if (strncmp(ext, ".geom", 5) == 0)
            return GL_GEOMETRY_SHADER_EXT;
    }

    return GL_FRAGMENT_SHADER;
}

//
//   Read a file's data into a string, and compile it using sh::Compile
//
bool CompileFile(char *fileName, ShHandle compiler, ShCompileOptions compileOptions)
{
    std::regex aqFishRegex(R"(aq-fish-nm\.frag$)");
    std::regex multiRegex(R"(multiview\.vert$)");
    const char *source;

    if (std::regex_search(fileName, aqFishRegex))
    {
        source = aqFishFrag.c_str();
    }
    else if (std::regex_search(fileName, multiRegex))
    {
        source = multiVert.c_str();
    }
    else
    {
        printf("Error: unable to open input file: %s\n", fileName);
        return false;
    }

    return sh::Compile(compiler, &source, 1, compileOptions);
}

void LogMsg(const char *msg, const char *name, const int num, const char *logName)
{
    printf("#### %s %s %d %s ####\n", msg, name, num, logName);
}

void PrintVariable(const std::string &prefix, size_t index, const sh::ShaderVariable &var)
{
    std::string typeName;
    switch (var.type)
    {
      case GL_FLOAT: typeName = "GL_FLOAT"; break;
      case GL_FLOAT_VEC2: typeName = "GL_FLOAT_VEC2"; break;
      case GL_FLOAT_VEC3: typeName = "GL_FLOAT_VEC3"; break;
      case GL_FLOAT_VEC4: typeName = "GL_FLOAT_VEC4"; break;
      case GL_INT: typeName = "GL_INT"; break;
      case GL_INT_VEC2: typeName = "GL_INT_VEC2"; break;
      case GL_INT_VEC3: typeName = "GL_INT_VEC3"; break;
      case GL_INT_VEC4: typeName = "GL_INT_VEC4"; break;
      case GL_UNSIGNED_INT: typeName = "GL_UNSIGNED_INT"; break;
      case GL_UNSIGNED_INT_VEC2: typeName = "GL_UNSIGNED_INT_VEC2"; break;
      case GL_UNSIGNED_INT_VEC3: typeName = "GL_UNSIGNED_INT_VEC3"; break;
      case GL_UNSIGNED_INT_VEC4: typeName = "GL_UNSIGNED_INT_VEC4"; break;
      case GL_BOOL: typeName = "GL_BOOL"; break;
      case GL_BOOL_VEC2: typeName = "GL_BOOL_VEC2"; break;
      case GL_BOOL_VEC3: typeName = "GL_BOOL_VEC3"; break;
      case GL_BOOL_VEC4: typeName = "GL_BOOL_VEC4"; break;
      case GL_FLOAT_MAT2: typeName = "GL_FLOAT_MAT2"; break;
      case GL_FLOAT_MAT3: typeName = "GL_FLOAT_MAT3"; break;
      case GL_FLOAT_MAT4: typeName = "GL_FLOAT_MAT4"; break;
      case GL_FLOAT_MAT2x3: typeName = "GL_FLOAT_MAT2x3"; break;
      case GL_FLOAT_MAT3x2: typeName = "GL_FLOAT_MAT3x2"; break;
      case GL_FLOAT_MAT4x2: typeName = "GL_FLOAT_MAT4x2"; break;
      case GL_FLOAT_MAT2x4: typeName = "GL_FLOAT_MAT2x4"; break;
      case GL_FLOAT_MAT3x4: typeName = "GL_FLOAT_MAT3x4"; break;
      case GL_FLOAT_MAT4x3: typeName = "GL_FLOAT_MAT4x3"; break;

      case GL_SAMPLER_2D: typeName = "GL_SAMPLER_2D"; break;
      case GL_SAMPLER_3D:
          typeName = "GL_SAMPLER_3D";
          break;
      case GL_SAMPLER_CUBE:
          typeName = "GL_SAMPLER_CUBE";
          break;
      case GL_SAMPLER_CUBE_SHADOW:
          typeName = "GL_SAMPLER_CUBE_SHADOW";
          break;
      case GL_SAMPLER_2D_SHADOW:
          typeName = "GL_SAMPLER_2D_ARRAY_SHADOW";
          break;
      case GL_SAMPLER_2D_ARRAY:
          typeName = "GL_SAMPLER_2D_ARRAY";
          break;
      case GL_SAMPLER_2D_ARRAY_SHADOW:
          typeName = "GL_SAMPLER_2D_ARRAY_SHADOW";
          break;
      case GL_SAMPLER_2D_MULTISAMPLE:
          typeName = "GL_SAMPLER_2D_MULTISAMPLE";
          break;
      case GL_IMAGE_2D:
          typeName = "GL_IMAGE_2D";
          break;
      case GL_IMAGE_3D:
          typeName = "GL_IMAGE_3D";
          break;
      case GL_IMAGE_CUBE:
          typeName = "GL_IMAGE_CUBE";
          break;
      case GL_IMAGE_2D_ARRAY:
          typeName = "GL_IMAGE_2D_ARRAY";
          break;

      case GL_INT_SAMPLER_2D:
          typeName = "GL_INT_SAMPLER_2D";
          break;
      case GL_INT_SAMPLER_3D:
          typeName = "GL_INT_SAMPLER_3D";
          break;
      case GL_INT_SAMPLER_CUBE:
          typeName = "GL_INT_SAMPLER_CUBE";
          break;
      case GL_INT_SAMPLER_2D_ARRAY:
          typeName = "GL_INT_SAMPLER_2D_ARRAY";
          break;
      case GL_INT_SAMPLER_2D_MULTISAMPLE:
          typeName = "GL_INT_SAMPLER_2D_MULTISAMPLE";
          break;
      case GL_INT_IMAGE_2D:
          typeName = "GL_INT_IMAGE_2D";
          break;
      case GL_INT_IMAGE_3D:
          typeName = "GL_INT_IMAGE_3D";
          break;
      case GL_INT_IMAGE_CUBE:
          typeName = "GL_INT_IMAGE_CUBE";
          break;
      case GL_INT_IMAGE_2D_ARRAY:
          typeName = "GL_INT_IMAGE_2D_ARRAY";
          break;

      case GL_UNSIGNED_INT_SAMPLER_2D:
          typeName = "GL_UNSIGNED_INT_SAMPLER_2D";
          break;
      case GL_UNSIGNED_INT_SAMPLER_3D:
          typeName = "GL_UNSIGNED_INT_SAMPLER_3D";
          break;
      case GL_UNSIGNED_INT_SAMPLER_CUBE:
          typeName = "GL_UNSIGNED_INT_SAMPLER_CUBE";
          break;
      case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
          typeName = "GL_UNSIGNED_INT_SAMPLER_2D_ARRAY";
          break;
      case GL_UNSIGNED_INT_ATOMIC_COUNTER:
          typeName = "GL_UNSIGNED_INT_ATOMIC_COUNTER";
          break;
      case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE:
          typeName = "GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE";
          break;
      case GL_UNSIGNED_INT_IMAGE_2D:
          typeName = "GL_UNSIGNED_INT_IMAGE_2D";
          break;
      case GL_UNSIGNED_INT_IMAGE_3D:
          typeName = "GL_UNSIGNED_INT_IMAGE_3D";
          break;
      case GL_UNSIGNED_INT_IMAGE_CUBE:
          typeName = "GL_UNSIGNED_INT_IMAGE_CUBE";
          break;
      case GL_UNSIGNED_INT_IMAGE_2D_ARRAY:
          typeName = "GL_UNSIGNED_INT_IMAGE_2D_ARRAY";
          break;

      case GL_SAMPLER_EXTERNAL_OES: typeName = "GL_SAMPLER_EXTERNAL_OES"; break;
      case GL_SAMPLER_EXTERNAL_2D_Y2Y_EXT:
          typeName = "GL_SAMPLER_EXTERNAL_2D_Y2Y_EXT";
          break;
      default: typeName = "UNKNOWN"; break;
    }

    printf("%s %u : name=%s, mappedName=%s, type=%s, arraySizes=", prefix.c_str(),
           static_cast<unsigned int>(index), var.name.c_str(), var.mappedName.c_str(),
           typeName.c_str());
    for (unsigned int arraySize : var.arraySizes)
    {
        printf("%u ", arraySize);
    }
    printf("\n");
    if (var.fields.size())
    {
        std::string structPrefix;
        for (size_t i = 0; i < prefix.size(); ++i)
            structPrefix += ' ';
        printf("%s  struct %s\n", structPrefix.c_str(), var.structName.c_str());
        structPrefix += "    field";
        for (size_t i = 0; i < var.fields.size(); ++i)
            PrintVariable(structPrefix, i, var.fields[i]);
    }
}

static void PrintActiveVariables(ShHandle compiler)
{
    const std::vector<sh::Uniform> *uniforms       = sh::GetUniforms(compiler);
    const std::vector<sh::Varying> *inputVaryings  = sh::GetInputVaryings(compiler);
    const std::vector<sh::Varying> *outputVaryings = sh::GetOutputVaryings(compiler);
    const std::vector<sh::Attribute> *attributes   = sh::GetAttributes(compiler);
    const std::vector<sh::OutputVariable> *outputs = sh::GetOutputVariables(compiler);
    for (size_t varCategory = 0; varCategory < 5; ++varCategory)
    {
        size_t numVars = 0;
        std::string varCategoryName;
        if (varCategory == 0)
        {
            numVars = uniforms->size();
            varCategoryName = "uniform";
        }
        else if (varCategory == 1)
        {
            numVars         = inputVaryings->size();
            varCategoryName = "input varying";
        }
        else if (varCategory == 2)
        {
            numVars         = outputVaryings->size();
            varCategoryName = "output varying";
        }
        else if (varCategory == 3)
        {
            numVars = attributes->size();
            varCategoryName = "attribute";
        }
        else
        {
            numVars         = outputs->size();
            varCategoryName = "output";
        }

        for (size_t i = 0; i < numVars; ++i)
        {
            const sh::ShaderVariable *var;
            if (varCategory == 0)
                var = &((*uniforms)[i]);
            else if (varCategory == 1)
                var = &((*inputVaryings)[i]);
            else if (varCategory == 2)
                var = &((*outputVaryings)[i]);
            else if (varCategory == 3)
                var = &((*attributes)[i]);
            else
                var = &((*outputs)[i]);

            PrintVariable(varCategoryName, i, *var);
        }
        printf("\n");
    }
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