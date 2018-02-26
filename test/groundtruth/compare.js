/**
 * Copyright (C) 2018 The ANGLE Project Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
'use strict';

const path = require('path');
const fs = require('fs');
const promisify = require('util').promisify;
const execAsync = promisify(require('child_process').exec);
const assert = require('assert');

const headValidatorModule = require('../../out/validator.js');

const multiviewVert = 'test/groundtruth/shaders/multiview.vert';
const aqFishFrag = 'test/groundtruth/shaders/aq-fish-nm.frag';


const tests = [
  {
    file: '',
    cmd: '',
    check: /^Usage: translate \[-i -o -u -l -p -b=e -b=g -b=h9 -x=i -x=d]/,
  },

  // aq-fish-nm.frag
  {
    file: aqFishFrag,
    cmd: '-i',
    check: /^#### BEGIN COMPILER 0 INFO LOG ####\n0:19: Code block/,
  }, {
    file: aqFishFrag,
    cmd: '-s=w -u',
    // eslint-disable-next-line max-len
    check: /#### BEGIN COMPILER 0 VARIABLES ####\nuniform 0 : name=lightColor, mappedName=_ulightColor, type=GL_FLOAT_VEC4, arraySizes=/,
  }, {
    file: aqFishFrag,
    cmd: '-s=w -o -b=h11',
    check: /cbuffer DriverConstants : register\(b1\)/,
  }, {
    file: aqFishFrag,
    cmd: '-s=w2 -o',
    // eslint-disable-next-line max-len
    check: /#### BEGIN COMPILER 0 OBJ CODE ####\nuniform mediump vec4 _ulightColor;\nvarying mediump vec4 _uv_position;/,
  },

  // multiview.vert
  {
    file: multiviewVert,
    cmd: '-s=w -u',
    // eslint-disable-next-line max-len
    check: /#### BEGIN COMPILER 0 INFO LOG ####\nERROR: 0:11: 'GL_OVR_multiview' : extension is not supported/,
  }, {
    file: multiviewVert,
    cmd: '-s=w -x=m -u',
    check: /^#### BEGIN COMPILER 0 INFO LOG ####\nERROR: unsupported shader version/,
  }, {
    file: multiviewVert,
    cmd: '-s=w2 -x=m -u',
    // eslint-disable-next-line max-len
    check: /#### BEGIN COMPILER 0 VARIABLES ####\nuniform 0 : name=uPerspective, mappedName=_uuPerspective, type=GL_FLOAT_MAT4, arraySizes=/,
  }, {
    file: multiviewVert,
    cmd: '-s=w2 -x=m -o -b=h9',
    // eslint-disable-next-line max-len
    check: /uniform int multiviewBaseViewLayerIndex : register\(c19\);\n#ifdef ANGLE_ENABLE_LOOP_FLATTEN/,
  },
];

function formatNumber(number, decimalPlaces = 0) {
  return number.toLocaleString(undefined, {maximumFractionDigits: decimalPlaces});
}

async function getHeadValidator() {
  return new Promise(resolve => {
    // Note: has then() but not a promise!
    headValidatorModule().then(instance => {
      // see https://github.com/kripken/emscripten/issues/5820
      instance.then = undefined;
      resolve(instance);
    });
  });
}

async function runGroundTruthCommand(file, cmd) {
  try {
    // eslint-disable-next-line max-len
    const output = await execAsync(`cd ${__dirname} && node translator.js ${cmd} ${file}`,
        {encoding: 'utf8'});
    return output.stdout;
  } catch (e) {
    return e.stdout;
  }
}

async function runHeadCommand(head, file, cmd) {
  const shaderType = file.endsWith('.vert') ?
      head.GL_VERTEX_SHADER : head.GL_FRAGMENT_SHADER;
  const filepath = path.resolve(__dirname, '../../', file);
  let shaderSrc = '';
  try {
    shaderSrc = fs.readFileSync(filepath, {encoding: 'utf8'});
  } catch (e) {}
  return head.validateShader(shaderSrc, shaderType, cmd);
}

async function run() {
  console.log('Sizes:');
  console.log('head:');
  const headWasmSize = fs.statSync(path.resolve(__dirname, '../../out/validator.wasm')).size;
  const headJsSize = fs.statSync(path.resolve(__dirname, '../../out/validator.js')).size;
  console.log(`  wasm: ${formatNumber(headWasmSize)} bytes`);
  console.log(`  js: ${formatNumber(headJsSize)} bytes`);

  console.log('original:');
  const origWasmSize = fs.statSync(path.resolve(__dirname, './translator.wasm')).size;
  const origJsSize = fs.statSync(path.resolve(__dirname, './translator.js')).size;
  console.log(`  wasm: ${formatNumber(origWasmSize)} bytes`);
  console.log(`  js: ${formatNumber(origJsSize)} bytes`);

  const head = await getHeadValidator();

  for (const test of tests) {
    console.log(`checking command \`${test.cmd} ${test.file}\`...`);

    const headOutput = await runHeadCommand(head, test.file, test.cmd);
    const gtOutput = await runGroundTruthCommand(test.file, test.cmd);

    assert(headOutput.length > 0);
    assert(test.check.test(headOutput));
    assert.strictEqual(headOutput.length, gtOutput.length);
    assert.strictEqual(headOutput, gtOutput);
  }

  console.log('complete!');
}

run();
