/**
 * Copyright (C) 2018 The ANGLE Project Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
'use strict';

/* global Module, ccall, _malloc, _free, setValue, getValue, Pointer_stringify, ENVIRONMENT_IS_NODE */

Module.locateFile = function(filename) {
  if (ENVIRONMENT_IS_NODE && filename.endsWith('.wasm')) {
    // In node, always resolve relative to js loader file regardless of cwd
    return require('path').resolve(__dirname, filename);
  }

  // TODO(bckenny): resolve on web
  return filename;
};

/**
 * @param {string} argv
 * @return {string} output of validator.
 */
Module.validateShader = function(argv) {
  // manually allocate slot for the resulting pointer
  const printLoc = _malloc(4);
  setValue(printLoc, 0, '*');

  ccall('ValidateShader', 'number', ['string', 'number'], [argv, printLoc]);

  const printLogLoc = getValue(printLoc, '*');
  // eslint-disable-next-line new-cap
  const printLog = Pointer_stringify(printLogLoc);

  // responsible for freeing log, if generated
  _free(printLoc);
  _free(printLogLoc);

  return printLog;
};

// export for Closure
Module['translateShader'] = Module.translateShader;
