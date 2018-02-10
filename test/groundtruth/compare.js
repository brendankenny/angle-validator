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

const commands = [
  '',
  '-i aq-fish-nm.frag',
  '-s=w -u aq-fish-nm.frag',
  '-s=w -o -b=h11 aq-fish-nm.frag',
  '-s=w2 -o aq-fish-nm.frag',
];

function formatNumber(number, decimalPlaces = 0) {
  return number.toLocaleString(undefined, {maximumFractionDigits: decimalPlaces});
}

async function runGroundTruthCommand(cmd) {
  const gtPath = path.resolve(__dirname, './translator.js');
  try {
    const output = await execAsync(`node ${gtPath} ${cmd}`, {encoding: 'utf8'});
    return output.stdout;
  } catch (e) {
    return e.stdout;
  }
}

async function runHeadCommand(cmd) {
  const runnerPath = path.resolve(__dirname, './runner.js');
  try {
    const output = await execAsync(`node ${runnerPath} ${cmd}`, {encoding: 'utf8'});
    return output.stdout;
  } catch (e) {
    return e.stdout;
  }
}

async function run() {
  console.log('Sizes:');
  console.log('head:');
  const headWasmSize = fs.statSync(path.resolve(__dirname, '../../translator.wasm')).size;
  const headJsSize = fs.statSync(path.resolve(__dirname, '../../translator.js')).size;
  console.log(`  wasm: ${formatNumber(headWasmSize)} bytes`);
  console.log(`  js: ${formatNumber(headJsSize)} bytes`);

  console.log('original:');
  const origWasmSize = fs.statSync(path.resolve(__dirname, './translator.wasm')).size;
  const origJsSize = fs.statSync(path.resolve(__dirname, './translator.js')).size;
  console.log(`  wasm: ${formatNumber(origWasmSize)} bytes`);
  console.log(`  js: ${formatNumber(origJsSize)} bytes`);

  for (const cmd of commands) {
    console.log('checking command `' + cmd + '`...');

    const headOutput = await runHeadCommand(cmd);
    const gtOutput = await runGroundTruthCommand(cmd);

    assert(headOutput.length > 0);
    assert.strictEqual(headOutput, gtOutput);
  }

  console.log('complete!');
}

run();
