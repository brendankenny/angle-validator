/**
 * Copyright (C) 2018 The ANGLE Project Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
// eslint-disable-next-line spaced-comment
/// <reference types="../types/emscripten-module" />
'use strict';

/* global Module, ccall, _malloc, _free, setValue, getValue, Pointer_stringify, ENVIRONMENT_IS_NODE */

// from gl.h
const GL_FRAGMENT_SHADER = 0x8B30;
const GL_VERTEX_SHADER = 0x8B31;
const GL_COMPUTE_SHADER = 0x91B9;
const GL_GEOMETRY_SHADER_EXT = 0x8DD9;

/**
 * @type {AngleValidator.CompileOptions}
 */
const defaultOptions = {
  use_precision_emulation: false, // -p

  // Extension enabled unless set to 0.
  GL_OES_EGL_image_external: false, // -x=i
  GL_OES_EGL_standard_derivatives: false, // -x=d
  ARB_texture_rectangle: false, // -x=r
  EXT_frag_depth: false, // -x=g
  EXT_shader_texture_lod: false, // -x=l
  EXT_shader_framebuffer_fetch: false, // -x=f
  NV_shader_framebuffer_fetch: false, // -x=n
  ARM_shader_framebuffer_fetch: false, // -x=a
  OVR_multiview: false, // -x=m
  YUV_target: false, // -x=y

  // Set to number desired; disabled if set to 0.
  EXT_blend_func_extended: 0, // -x=b[NUM]
  EXT_draw_buffers: 0, // -x=w[NUM]
};

/**
 * Shared with enum in shader-validator.cpp
 * @type {AngleValidator.InputType}
 */
const INPUT_TYPES = {
  GLES: 0,
  WEBGL: 1,
};

/**
 * Shared with enum in shader-validator.cpp
 * @type {AngleValidator.OutputType}
 */
const OUTPUT_TYPES = {
  GLES: 0,
  GLSL: 1,
  HLSL: 2,
};

/**
 * @param {Partial<AngleValidator.CompileOptions>} opts
 * @return {Int32Array}
 */
function constructCompileOptionsStruct(opts) {
  const defaultKeys = /** @type {Array<keyof AngleValidator.CompileOptions>} */ (Object.keys(defaultOptions));
  const buffer = new Int32Array(defaultKeys.length);

  defaultKeys.forEach(function(optionName, index) {
    const defaultValue = defaultOptions[optionName];
    const optValue = opts[optionName];
    const value = optValue !== undefined ? optValue : defaultValue;

    if (typeof defaultValue === 'number') {
      buffer[index] = Number(value);
    } else {
      buffer[index] = value ? 1 : 0;
    }
  });

  return buffer;
}

/**
 * @param {AngleValidator.ShaderInputType} inputType
 * @return {Int32Array}
 */
function constructInputOptions(inputType) {
  const inputTypeBuffer = new Int32Array(2);
  inputTypeBuffer[0] = inputType.spec;

  if (inputType.spec === INPUT_TYPES.GLES) {
    // default to GLES 2
    inputTypeBuffer[1] = inputType.version || 2;
  } else {
    // default to WebGL 1.0
    inputTypeBuffer[1] = inputType.version || 1;
  }

  return inputTypeBuffer;
}

/**
 * @param {AngleValidator.ShaderOutputType=} outputType
 * @return {Int32Array}
 */
function constructOutputOptions(outputType) {
  const outputTypeBuffer = new Int32Array(2);

  if (!outputType) {
    outputTypeBuffer[0] = OUTPUT_TYPES.GLES;
    return outputTypeBuffer;
  }

  outputTypeBuffer[0] = outputType.spec;

  if (outputType.spec === OUTPUT_TYPES.GLSL) {
    // default is 0 (compatibility profile)
    outputTypeBuffer[1] = outputType.version || 2;
  } else if (outputType.spec === OUTPUT_TYPES.HLSL) {
    // default to HLSL 9
    outputTypeBuffer[1] = outputType.version || 9;
  }

  return outputTypeBuffer;
}

/**
 * Override default Module.locateFile to find .wasm file relative to .js file.
 * @param {string} filename
 * @return {string}
 */
Module.locateFile = function(filename) {
  if (ENVIRONMENT_IS_NODE && filename.endsWith('.wasm')) {
    // In node, always resolve relative to js loader file regardless of cwd
    return require('path').resolve(__dirname, filename);
  }

  // TODO(bckenny): resolve on web?
  return filename;
};

/**
 * @param {string} shaderSrc
 * @param {number=} shaderType
 * @param {string} argv
 * @return {string} output of validator.
 */
Module.validateShader = function(shaderSrc, shaderType, argv) {
  shaderType = shaderType || GL_FRAGMENT_SHADER;

  // manually allocate slot for the resulting pointer
  const printLoc = _malloc(4);
  setValue(printLoc, 0, '*');

  ccall('ValidateShader', 'number',
      ['string', 'number', 'string', 'number'],
      [shaderSrc, shaderType, argv, printLoc]);

  const printLogLoc = getValue(printLoc, '*');
  // eslint-disable-next-line new-cap
  const printLog = Pointer_stringify(printLogLoc);

  // responsible for freeing log, if generated
  _free(printLoc);
  _free(printLogLoc);

  return printLog;
};

// export for Closure
Module['GL_FRAGMENT_SHADER'] = GL_FRAGMENT_SHADER;
Module['GL_VERTEX_SHADER'] = GL_VERTEX_SHADER;
Module['GL_COMPUTE_SHADER'] = GL_COMPUTE_SHADER;
Module['GL_GEOMETRY_SHADER_EXT'] = GL_GEOMETRY_SHADER_EXT;
Module['validateShader'] = Module.validateShader;
