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

const GLSL_ES2 = 'e2';
const GLSL_ES3 = 'e3';
const GLSL_ES31 = 'e31';
const WEBGL1 = 'w';
const WEBGL2 = 'w2';
const INPUT_TYPES = [GLSL_ES2, GLSL_ES3, GLSL_ES31, WEBGL1, WEBGL2];

const GLSL_ES = 'e';
const GLSL = 'g';
const GLSL_VERSIONS = [130, 140, 150, 330, 400, 410, 420, 430, 440, 450].map(version => {
  return GLSL + version;
});
const HLSL9 = 'h9';
const HLSL11 = 'h11';
const OUTPUT_TYPES = [GLSL_ES, GLSL, ...GLSL_VERSIONS, HLSL9, HLSL11];

const tests = [
  // aq-fish-nm.frag
  {
    file: aqFishFrag,
    cmd: {
      input: WEBGL1,
      output: HLSL11,
    },
    check: /cbuffer DriverConstants : register\(b1\)/,
  }, {
    file: aqFishFrag,
    cmd: {
      input: WEBGL2,
    },
    // eslint-disable-next-line max-len
    check: /#### BEGIN COMPILER 0 OBJ CODE ####\nuniform mediump vec4 _ulightColor;\nvarying mediump vec4 _uv_position;/,
  },

  // multiview.vert
  {
    file: multiviewVert,
    cmd: {
      input: WEBGL1,
    },
    // eslint-disable-next-line max-len
    check: /#### BEGIN COMPILER 0 INFO LOG ####\nERROR: 0:11: 'GL_OVR_multiview' : extension is not supported/,
  }, {
    file: multiviewVert,
    cmd: {
      input: WEBGL1,
      OVR_multiview: true,
    },
    check: /^#### BEGIN COMPILER 0 INFO LOG ####\nERROR: unsupported shader version/,
  }, {
    file: multiviewVert,
    cmd: {
      input: WEBGL2,
      OVR_multiview: true,
      output: HLSL9,
    },
    // eslint-disable-next-line max-len
    check: /uniform int multiviewBaseViewLayerIndex : register\(c19\);\n#ifdef ANGLE_ENABLE_LOOP_FLATTEN/,
  },
];

function formatNumber(number, decimalPlaces = 0) {
  return number.toLocaleString(undefined, {maximumFractionDigits: decimalPlaces});
}

function createArgv(options) {
  const args = [];

  const inputType = options.input ? options.input : GLSL_ES2;
  if (!INPUT_TYPES.includes(inputType)) {
    throw new Error(inputType + ' not a supported shader input type');
  }
  args.push('-s=' + inputType);

  // TODO(bckenny): no output should be just validation?
  const outputType = options.output ? options.output : GLSL_ES;
  if (!OUTPUT_TYPES.includes(outputType)) {
    throw new Error(outputType + ' not a supported shader output type');
  }
  args.push('-o', '-b=' + outputType);

  if (options.precisionEmulation) {
    args.push('-p');
  }

  const BOOL_EXTENSIONS = {
    GL_OES_EGL_image_external: 'i',
    GL_OES_EGL_standard_derivatives: 'd',
    ARB_texture_rectangle: 'r',
    EXT_frag_depth: 'g',
    EXT_shader_texture_lod: 'l',
    EXT_shader_framebuffer_fetch: 'f',
    NV_shader_framebuffer_fetch: 'n',
    ARM_shader_framebuffer_fetch: 'a',
    OVR_multiview: 'm',
    YUV_target: 'y',
  };

  const NUM_EXTENSIONS = {
    EXT_blend_func_extended: 'b',
    EXT_draw_buffers: 'w',
  };

  for (const [key, flag] of Object.entries(BOOL_EXTENSIONS)) {
    if (options[key]) {
      args.push('-x=' + flag);
    }
  }

  for (const [key, flag] of Object.entries(NUM_EXTENSIONS)) {
    const num = options[key];
    if (num && Number.isFinite(Number(num))) {
      args.push('-x=' + flag + Number(num));
    }
  }

  return args.join(' ');
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
    let headCmd = '';
    let gtCmd = '';
    if (test.cmd) {
      const cmd = createArgv(test.cmd);
      headCmd = cmd;
      gtCmd = cmd;
      console.log(`checking command \`${cmd} ${test.file}\`...`);
    } else {
      headCmd = createArgv(test.headCmd);
      gtCmd = createArgv(test.gtCmd);

      console.log(`checking head \`${headCmd} ${test.file}\`...`);
      console.log(`      vs gt \`${gtCmd} ${test.file}\`...`);
    }

    const headOutput = await runHeadCommand(head, test.file, headCmd);
    const gtOutput = await runGroundTruthCommand(test.file, gtCmd);

    assert(headOutput.length > 0);
    assert(test.check.test(headOutput), `failed regex ${test.check}`);
    assert.strictEqual(headOutput.length, gtOutput.length);
    assert.strictEqual(headOutput, gtOutput);
  }

  console.log('complete!');
}

run().catch(err => {
  console.error('failed, with message `' + err.message + '`');
  throw err;
});
