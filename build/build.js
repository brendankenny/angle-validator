/**
 * Copyright (C) 2018 The ANGLE Project Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
'use strict';

const spawnSync = require('child_process').spawnSync;

const config = require('./build-config.json');
const anglePath = 'angle/';
const validatorSrc = 'src/shader-translator.cpp';

const EMSDK_INIT = 'source ./emsdk/emsdk_env.sh';
const BUILD_ECHO = 'echo Compiling validator...';
const COMPILER = 'emcc';
const OUTPUT_ARGS = [
  '-o', 'validator.js',
  // '--embed-file', 'test/groundtruth/shaders/aq-fish-nm.frag',
  // '--embed-file', 'test/groundtruth/shaders/multiview.vert',
  '-O1',
  '-s', 'WASM=1',
  '-s', 'MODULARIZE=1',
  '-s', 'INVOKE_RUN=0',
  '-s', 'NO_FILESYSTEM=1',
  '-g2',
];

// eslint-disable-next-line no-unused-vars
const gtSrc = anglePath + 'samples/shader_translator/shader_translator.cpp';
// eslint-disable-next-line no-unused-vars
const GT_ARGS = [
  '-o', 'translator.js',
  '--embed-file', 'test/groundtruth/shaders/aq-fish-nm.frag',
  '--embed-file', 'test/groundtruth/shaders/multiview.vert',
  '-Os',
  '-s', 'WASM=1',
];

function run() {
  const commandEcho = `echo ${COMPILER} -D... -I... ... ${validatorSrc} ${OUTPUT_ARGS.join(' ')}`;

  const args = [].concat(
    config.cflags_cc,
    config.defines.map(d => '-D' + d),
    config.include_dirs.map(i => '-I' + anglePath + i),
    config.sources.map(s => anglePath + s),
    validatorSrc,
    OUTPUT_ARGS);

  const command = [EMSDK_INIT, BUILD_ECHO, commandEcho, COMPILER].join(' && ');
  spawnSync(command, args, {
    encoding: 'utf8',
    shell: true,
    stdio: ['inherit', 'inherit', 'inherit'],
  });

  console.log('Complete.');
}

run();
