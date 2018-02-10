/**
 * Copyright (C) 2018 The ANGLE Project Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
'use strict';

const path = require('path');

// look for latest build of translator
const dir = path.resolve(__dirname, '../../');

process.chdir(dir);
const translator = require(path.resolve(dir, './translator.js'));
translator().then(translatorModule => {
  // call main() with argv
  translatorModule.callMain(process.argv.slice(2));
});
