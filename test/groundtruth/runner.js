/**
 * Copyright (C) 2018 The ANGLE Project Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
'use strict';

const path = require('path');

// look for latest build of validator
const dir = path.resolve(__dirname, '../../');

process.chdir(dir);
const validator = require(path.resolve(dir, './validator.js'));
validator().then(validatorModule => {
  // call main() with argv
  validatorModule.callMain(process.argv.slice(2));
});
