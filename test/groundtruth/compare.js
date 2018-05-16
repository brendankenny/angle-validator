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

// TODO(bckenny): get from module instead of c/p
/**
 * Shared with enum in shader-validator.cpp
 * @type {AngleValidator.InputType}
 */
const INPUT_TYPE = {
  GLES: 0,
  WEBGL: 1,
};

/**
 * Shared with enum in shader-validator.cpp
 * @type {AngleValidator.OutputType}
 */
const OUTPUT_TYPE = {
  GLES: 0,
  GLSL: 1,
  HLSL: 2,
};

const GLSL_ES2 = 'e2';
const GLSL_ES3 = 'e3';
const GLSL_ES31 = 'e31';
const WEBGL1 = 'w';
const WEBGL2 = 'w2';
const INPUT_TYPES = [GLSL_ES2, GLSL_ES3, GLSL_ES31, WEBGL1, WEBGL2];
const INPUT_OPTIONS = {
  [GLSL_ES2]: {spec: INPUT_TYPE.GLES, version: 2},
  [GLSL_ES3]: {spec: INPUT_TYPE.GLES, version: 3},
  [GLSL_ES31]: {spec: INPUT_TYPE.GLES, version: 31},
  [WEBGL1]: {spec: INPUT_TYPE.WEBGL, version: 1},
  [WEBGL2]: {spec: INPUT_TYPE.WEBGL, version: 2},
}

const GLSL_ES = 'e';
const GLSL = 'g';
const GLSL_VERSIONS = [130, 140, 150, 330, 400, 410, 420, 430, 440, 450].map(version => {
  return GLSL + version;
});
const HLSL9 = 'h9';
const HLSL11 = 'h11';
const OUTPUT_TYPES = [GLSL_ES, GLSL, ...GLSL_VERSIONS, HLSL9, HLSL11];
const OUTPUT_OPTIONS = {
  [GLSL_ES]: {spec: OUTPUT_TYPE.GLES},
  [GLSL]: {spec: OUTPUT_TYPE.GLSL, version: 3},
  g130: {spec: OUTPUT_TYPE.GLSL, version: 130},
  g140: {spec: OUTPUT_TYPE.GLSL, version: 140},
  g150: {spec: OUTPUT_TYPE.GLSL, version: 150},
  g330: {spec: OUTPUT_TYPE.GLSL, version: 330},
  g400: {spec: OUTPUT_TYPE.GLSL, version: 400},
  g410: {spec: OUTPUT_TYPE.GLSL, version: 410},
  g420: {spec: OUTPUT_TYPE.GLSL, version: 420},
  g430: {spec: OUTPUT_TYPE.GLSL, version: 430},
  g440: {spec: OUTPUT_TYPE.GLSL, version: 440},
  g450: {spec: OUTPUT_TYPE.GLSL, version: 450},
  [HLSL9]: {spec: OUTPUT_TYPE.HLSL, version: 9},
  [HLSL11]: {spec: OUTPUT_TYPE.HLSL, version: 11},
}

// eslint-disable-next-line max-len
const ERROR_LOG_EXTRACT = /#### BEGIN COMPILER 0 INFO LOG ####\n([^]*)\n\n#### END COMPILER 0 INFO LOG ####/;
// eslint-disable-next-line max-len
const TRANSLATED_EXTRACT = /#### BEGIN COMPILER 0 OBJ CODE ####\n([^]*)\n\n#### END COMPILER 0 OBJ CODE ####/;

const tests = [
  // aq-fish-nm.frag
  {
    file: aqFishFrag,
    cmd: {
      input: WEBGL2,
      output: GLSL_ES,
    },
    translatedCheck: /uniform mediump vec4 _ulightColor;\nvarying mediump vec4 _uv_position;/,
  }, {
    file: aqFishFrag,
    cmd: {
      input: WEBGL2,
      output: GLSL_ES,
      use_precision_emulation: true,
    },
    translatedCheck: /^highp float angle_frm\(in highp float x\) {/,
  }, {
    file: aqFishFrag,
    cmd: {
      input: WEBGL2,
      output: 'g330',
    },
    translatedCheck: /^#version 330\n#extension GL_ARB_gpu_shader5 : enable/,
  }, {
    file: aqFishFrag,
    cmd: {
      input: WEBGL1,
      output: HLSL9,
    },
    translatedCheck: /^float3x3 mat3_ctor\(float3 x0, float3 x1, float3 x2\)/,
  }, {
    file: aqFishFrag,
    cmd: {
      input: WEBGL1,
      output: HLSL11,
    },
    translatedCheck: /cbuffer DriverConstants : register\(b1\)/,
  }, {
    file: aqFishFrag,
    cmd: {
      input: WEBGL1,
      output: HLSL9,
      use_precision_emulation: true,
    },
    errorCheck: /^ERROR: Precision emulation not supported for this output type./,
  }, {
    file: aqFishFrag,
    cmd: {
      input: WEBGL1,
      output: HLSL11,
      use_precision_emulation: true,
    },
    translatedCheck: /^float1 angle_frm\(float1 v\) {\n *v = clamp\(v, -65504\.0, 65504\.0\);/,
  },

  // multiview.vert
  {
    file: multiviewVert,
    cmd: {
      input: WEBGL1,
    },
    errorCheck: /ERROR: 0:11: 'GL_OVR_multiview' : extension is not supported/,
  }, {
    file: multiviewVert,
    cmd: {
      input: WEBGL1,
      OVR_multiview: true,
    },
    errorCheck: /ERROR: unsupported shader version/,
  }, {
    file: multiviewVert,
    cmd: {
      input: WEBGL2,
      OVR_multiview: true,
      output: HLSL9,
    },
    // eslint-disable-next-line max-len
    translatedCheck: /uniform int multiviewBaseViewLayerIndex : register\(c19\);\n#ifdef ANGLE_ENABLE_LOOP_FLATTEN/,
  }, {
    file: multiviewVert,
    cmd: {
      input: WEBGL2,
      OVR_multiview: true,
      output: HLSL11,
    },
    // eslint-disable-next-line max-len
    translatedCheck: /cbuffer DriverConstants : register\(b1\)\n{\n *float4 dx_ViewAdjust : packoffset\(c1\);/,
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

  const outputType = options.output ? options.output : GLSL_ES;
  if (!OUTPUT_TYPES.includes(outputType)) {
    throw new Error(outputType + ' not a supported shader output type');
  }
  args.push('-o', '-b=' + outputType);

  if (options.use_precision_emulation) {
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

function createOptions(options) {
  const inputTypeCmd = options.input ? options.input : GLSL_ES2;
  if (!INPUT_TYPES.includes(inputTypeCmd)) {
    throw new Error(inputType + ' not a supported shader input type');
  }
  const inputOptions = {...INPUT_OPTIONS[inputTypeCmd]};
  
  const outputTypeCmd = options.output ? options.output : GLSL_ES;
  if (!OUTPUT_TYPES.includes(outputTypeCmd)) {
    throw new Error(outputType + ' not a supported shader output type');
  }
  const outputOptions = {...OUTPUT_OPTIONS[outputTypeCmd]};

  // TODO(bckenny): should validate rather than just blindly copy options
  const compilerOptions = {...options};

  return [inputOptions, outputOptions, compilerOptions];
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
  let stdout = '';

  try {
    // eslint-disable-next-line max-len
    const output = await execAsync(`cd ${__dirname} && node translator.js ${cmd} ${file}`,
        {encoding: 'utf8'});
    stdout = output.stdout;
  } catch (e) {
    stdout = e.stdout;
  }

  const errorLog = ERROR_LOG_EXTRACT.exec(stdout);
  const translatedCode = TRANSLATED_EXTRACT.exec(stdout);
  return {
    errorLog: errorLog && errorLog[1],
    translatedCode: translatedCode && translatedCode[1],
  };
}

async function runHeadCommand(head, file, inputOpts, outputOpts, compileOpts) {
  const shaderType = file.endsWith('.vert') ?
      head.GL_VERTEX_SHADER : head.GL_FRAGMENT_SHADER;
  const filepath = path.resolve(__dirname, '../../', file);
  let shaderSrc = '';
  try {
    shaderSrc = fs.readFileSync(filepath, {encoding: 'utf8'});
  } catch (e) {}


  const output = head.validateShader(shaderSrc, shaderType, inputOpts, outputOpts, compileOpts);

  const errorLog = ERROR_LOG_EXTRACT.exec(output);
  const translatedCode = TRANSLATED_EXTRACT.exec(output);
  return {
    errorLog: errorLog && errorLog[1],
    translatedCode: translatedCode && translatedCode[1],
  };
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
    let headOptions = [];
    let headCmd = '';
    let gtCmd = '';
    if (test.cmd) {
      const cmd = createArgv(test.cmd);
      headCmd = cmd;
      gtCmd = cmd;
      headOptions = createOptions(test.cmd);
      console.log(`checking command \`${cmd} ${test.file}\`...`);
      // console.log('equivalent options struct: ', JSON.stringify(headOptions));
    } else {
      headCmd = createArgv(test.headCmd);
      gtCmd = createArgv(test.gtCmd);
      headOptions = createOptions(test.cmd);

      console.log(`checking head \`${headCmd} ${test.file}\`...`);
      console.log(`      vs gt \`${gtCmd} ${test.file}\`...`);
      // console.log('head options struct: ', JSON.stringify(headOptions));
    }

    const headOutput = await runHeadCommand(head, test.file, ...headOptions);
    const gtOutput = await runGroundTruthCommand(test.file, gtCmd);

    if (!test.errorCheck) {
      assert.strictEqual(headOutput.errorLog, null);
    } else {
      assert(test.errorCheck.test(headOutput.errorLog), `errorLog failed regex ${test.errorCheck}`);
    }

    if (!test.translatedCheck) {
      assert.strictEqual(headOutput.translatedCode, null);
    } else {
      assert(test.translatedCheck.test(headOutput.translatedCode),
          `translatedCode failed regex ${test.translatedCheck}`);
    }

    assert.strictEqual(headOutput.errorLog, gtOutput.errorLog);
    assert.strictEqual(headOutput.translatedCode, gtOutput.translatedCode);
  }

  console.log('complete!');
}

run().catch(err => {
  console.error('failed, with message `' + err.message + '`');
  throw err;
});
