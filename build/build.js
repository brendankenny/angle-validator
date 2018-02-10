/**
 * Copyright (C) 2018 The ANGLE Project Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
'use strict';

const spawnSync = require('child_process').spawnSync;

const config = require('./build-config.json');
const anglePath = 'angle/';
const translatorSrc = 'src/shader-translator.cpp';

const EMSDK_INIT = 'source ./emsdk/emsdk_env.sh';
const BUILD_ECHO = 'echo Comipling translator...';
const COMPILER = 'emcc';
const OUTPUT_ARGS = [
  '-o', 'translator.js',
  '--embed-file', 'aq-fish-nm.frag',
  '-Os',
  '-s', 'WASM=1',
  '-s', 'MODULARIZE=1',
  '-s', 'INVOKE_RUN=0',
];

function run() {
  const commandEcho = `echo ${COMPILER} -D... -I... ... ${translatorSrc} ${OUTPUT_ARGS.join(' ')}`;

  const args = [].concat(
    config.cflags_cc,
    config.defines.map(d => '-D' + d),
    config.include_dirs.map(i => '-I' + anglePath + i),
    config.sources.map(s => anglePath + s),
    translatorSrc,
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
