/**
 * Copyright (C) 2018 The ANGLE Project Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
'use strict';

/* global Module, ccall, _malloc, _free, setValue, getValue, Pointer_stringify, ENVIRONMENT_IS_NODE */

// from gl.h
const GL_FRAGMENT_SHADER = 0x8B30;
const GL_VERTEX_SHADER = 0x8B31;
const GL_COMPUTE_SHADER = 0x91B9;
const GL_GEOMETRY_SHADER_EXT = 0x8DD9;

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
 * @param {number=} shaderType
 * @param {string} argv
 * @return {string} output of validator.
 */
Module.validateShader = function(shaderType, argv) {
  shaderType = shaderType || GL_FRAGMENT_SHADER;

  // manually allocate slot for the resulting pointer
  const printLoc = _malloc(4);
  setValue(printLoc, 0, '*');

  ccall('ValidateShader', 'number', ['number', 'string', 'number'], [shaderType, argv, printLoc]);

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
Module['translateShader'] = Module.translateShader;
