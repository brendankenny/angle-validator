/**
 * Copyright (C) 2018 The ANGLE Project Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Extern definitions intended for code that will be compiled by emscripten into
 * the module js, which means a number of emscripten functions will be in the
 * (apparent) global scope.
 */

// TODO(bckenny): figure out how to limit to pre-module.js

declare global {
  var ENVIRONMENT_IS_NODE: boolean;
  function getValue(ptr: number, type: string, noSafe?: boolean): number;
  function setValue(ptr: number, value: any, type: string, noSafe?: boolean): void;
  function ccall(ident: string, returnType: string | null, argTypes: string[], args: any[]): any;
  function Pointer_stringify(ptr: number, length?: number): string;

  function _malloc(size: number): number;
  function _free(ptr: number): void;

  var Module: {
    locateFile(filename: string): string;
    validateShader(shaderSrc: string, shaderType: number|undefined, argv: string): string;
    GL_FRAGMENT_SHADER: number;
    GL_VERTEX_SHADER: number;
    GL_COMPUTE_SHADER: number;
    GL_GEOMETRY_SHADER_EXT: number;
    [p: string]: any;
  }

  module AngleValidator {
    interface CompileOptions {
      use_precision_emulation: boolean; // -p

      // Extension enabled unless set to 0.
      GL_OES_EGL_image_external: boolean; // -x=i
      GL_OES_EGL_standard_derivatives: boolean; // -x=d
      ARB_texture_rectangle: boolean; // -x=r
      EXT_frag_depth: boolean; // -x=g
      EXT_shader_texture_lod: boolean; // -x=l
      EXT_shader_framebuffer_fetch: boolean; // -x=f
      NV_shader_framebuffer_fetch: boolean; // -x=n
      ARM_shader_framebuffer_fetch: boolean; // -x=a
      OVR_multiview: boolean; // -x=m
      YUV_target: boolean; // -x=y
    
      // Set to number desired; disabled if set to 0.
      EXT_blend_func_extended: number; // -x=b[NUM]
      EXT_draw_buffers: number; // -x=w[NUM]
    }

    interface InputType {
      GLES: 0;
      WEBGL: 1;
    }

    type ShaderInputType = {
      spec: InputType['GLES'];
      version?: 2 | 3 | 31;
    } | {
      spec: InputType['WEBGL'];
      version?: 1 | 2 | 3;
    };

    interface OutputType {
      GLES: 0;
      GLSL: 1;
      HLSL: 2;
    }

    type ShaderOutputType = {
      spec: OutputType['GLES'];
    } | {
      spec: OutputType['GLSL'];
      version?: 130 | 140 | 150 | 330 | 400 | 410 | 420 | 430 | 440 | 450;
    } | {
      spec: OutputType['HLSL'];
      version?: 9 | 11;
    };
  }
}

// empty export to keep file a module
export {};
