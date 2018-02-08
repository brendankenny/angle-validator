/**
 * Copyright (C) 2018 The ANGLE Project Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
'use strict';

const path = require('path');
const fs = require('fs');
const DEBUG = true;

const log = DEBUG ? console.log : () => {};

// Generated by `yarn gen` (see package.json)
const angleGenPath = path.resolve(__dirname, '../angle/out/json/project.json');

const DEP_TARGETS = ['//:translator'];
const completedTargets = Symbol('completedTargets');

/**
 * Properties to collect from build file, with functions to sanitize values.
 * @type {!Object<string, function(!Array<string>): !Array<string>}
 */
const TARGET_PROPS = {
  cflags_cc: identity,
  defines: identity,
  include_dirs: filterIncludes,
  sources: filterSources,
};

/**
 * Filter/sanitize an array of source paths.
 * @param {!Array<string>} sources
 * @return {!Array<string>}
 */
function filterSources(sources) {
  return sources
    // drop initial '//' from paths
    .map(src => src.substring(2))
    .filter(src => {
      // ignore header and grammar files
      return (src.endsWith('.cpp') || src.endsWith('.cc')) &&
          // ignore unnecessary system-specific files with no emscripten support
          !/system_utils_(linux|mac|win).cpp$/.test(src);
    });
}

/**
 * Filter/sanitize an array of include paths.
 * @param {!Array<string>} includes
 * @return {!Array<string>}
 */
function filterIncludes(includes) {
  return includes
    // drop initial '//' from paths
    .map(inc => inc.substring(2))
    .filter(inc => {
      // ignore '' and header files that would be placed in angle output dir
      return inc.length > 0 && !inc.startsWith('out/json');
    });
}

/**
 * Return array of strings unchanged.
 * @param {!Array<string>} arr
 * @return {!Array<string>}
 */
function identity(arr) {
  return arr;
}

/**
 * Depth first walk of dependencies from target `targetName`, taking the union
 * of the values of TARGET_PROPS as we go. Output on `extracted`.
 * @param {{targets: !Object<string, !Object<string, (!Array<string>|undefined)>}} buildInfo
 * @param {string} targetName
 * @param {!Object<string, !Set<string>>} extracted
 * @param {number=} depth
 */
function gatherDeps(buildInfo, targetName, extracted, depth = 0) {
  // Track already walked deps via `completedTargets` symbol.
  if (!extracted[completedTargets]) {
    extracted[completedTargets] = new Set();
  }

  const logName = extracted[completedTargets].has(targetName) ? `(${targetName})` : targetName;
  log('  '.repeat(depth) + logName);

  const target = buildInfo.targets[targetName];
  if (!target) {
    throw new Error(`no ${targetName} found`);
  }

  if (!target.deps) {
    log('  '.repeat(depth) + `  ${targetName} has no deps`);
  } else {
    for (const depName of target.deps) {
      if (!depName.startsWith('//:')) {
        throw new Error(`${targetName} has bad dep name ${depName}`);
      }

      // recurse over dependencies.
      gatherDeps(buildInfo, depName, extracted, depth + 1);
    }
  }

  // Descend into already-walked deps to print full tree, but any target props
  // will have already been added, so don't try again.
  if (extracted[completedTargets].has(targetName)) {
    return;
  }
  extracted[completedTargets].add(targetName);

  // Loop over desired properties and add filtered values to ongoing unions.
  for (const [propName, filterValues] of Object.entries(TARGET_PROPS)) {
    if (!extracted[propName]) {
      extracted[propName] = new Set();
    }

    if (!target[propName]) {
      log('  '.repeat(depth) + `  - no ${propName}`);
      continue;
    }

    for (const propValue of filterValues(target[propName])) {
      extracted[propName].add(propValue);
    }
  }
}

let angleGen;
try {
  angleGen = require(angleGenPath);
} catch (e) {
  console.warn(`build info not found at \`${angleGenPath}\`. Run \`yarn gen\` to generate.`);
  process.exit(1);
}

const extracted = {};
for (const targetName of DEP_TARGETS) {
  gatherDeps(angleGen, targetName, extracted);
}

// Convert sets to arrays for JSON.
for (const propName of Object.keys(extracted)) {
  extracted[propName] = [...extracted[propName]];
}

fs.writeFileSync('./build/build-config.json', JSON.stringify(extracted, null, 2));
