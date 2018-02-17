/**
 * Copyright (C) 2018 The ANGLE Project Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
'use strict';

const path = require('path');

// look for latest build of validator
const dir = path.resolve(__dirname, '../../out/');

// process.chdir(dir);
const validator = require(path.resolve(dir, './validator.js'));
validator().then(validatorModule => {
  const argv = process.argv.slice(2).join(' ');
  const output = validatorModule.validateShader(argv);
  process.stdout.write(output);
});
