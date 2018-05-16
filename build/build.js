/**
 * Copyright (C) 2018 The ANGLE Project Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
'use strict';

const fs = require('fs');
const path = require('path');
const spawnSync = require('child_process').spawnSync;
const mkdirp = require('mkdirp');

const config = require('./build-config.json');
const anglePath = 'angle/';

const EMSDK_INIT = 'source ./emsdk/emsdk_env.sh';
const BUILD_ECHO = 'echo Compiling...';
const COMPILER = 'emcc';

const validatorSrc = 'src/shader-validator.cpp';
const OUTPUT_ARGS = [
  '-o', 'out/validator.js',
  '-Os',
  '-s', 'WASM=1',
  '-s', 'MODULARIZE=1',
  '-s', 'INVOKE_RUN=0',
  '-s', 'NO_FILESYSTEM=1',
  '-s', 'EXPORTED_FUNCTIONS=[\\"_ValidateShader\\"]',
  '--pre-js', 'src/pre-module.js',
  // '-g2', // retain function names for debugging
];

const gtSrc = anglePath + 'samples/shader_translator/shader_translator.cpp';
const GT_OUTPUT_ARGS = [
  '-o', 'test/groundtruth/translator.js',
  '--embed-file', 'test/groundtruth/shaders/aq-fish-nm.frag',
  '--embed-file', 'test/groundtruth/shaders/multiview.vert',
  '-Os',
  '-s', 'WASM=1',
];

/**
 * Find output path and ensure chain of directories is available.
 * @param {Array<string>} args
 */
function ensureOutputPath(args) {
  const index = args.indexOf('-o') + 1;
  if (index === 0) {
    throw new Error('output path not found in' + JSON.stringify(args));
  }

  const outPath = path.dirname(args[index]);

  // create out directory, if needed
  try {
    fs.accessSync(outPath, fs.constants.R_OK | fs.constants.W_OK);
  } catch (err) {
    console.warn(`\`${outPath}/\` does not exist. Creating...`);
    mkdirp.sync(outPath);
  }
}

function run(src = validatorSrc, outputArgs = OUTPUT_ARGS) {
  ensureOutputPath(outputArgs);

  const commandEcho = `echo ${COMPILER} -D... -I... ... ${src} ${outputArgs.join(' ')}`;

  const args = /** @type {Array<string>} */ ([]).concat(
    config.cflags_cc,
    config.defines.map(d => '-D' + d),
    config.include_dirs.map(i => '-I' + anglePath + i),
    config.sources.map(s => anglePath + s),
    src,
    outputArgs);

  const command = [EMSDK_INIT, BUILD_ECHO, commandEcho, COMPILER].join(' && ');
  spawnSync(command, args, {
    encoding: 'utf8',
    shell: true,
    stdio: ['inherit', 'inherit', 'inherit'],
  });

  console.log('Complete.');
}

if (process.argv.includes('--gt')) {
  run(gtSrc, GT_OUTPUT_ARGS);
} else if (process.argv.includes('--quick')) {
  run(validatorSrc, OUTPUT_ARGS.map(arg => /^-O[\dsz]$/.test(arg) ? '-O1' : arg));
} else {
  run();
}
