/******************************************************************************
 *  This file is part of Yune".
 *
 *  Copyright (C) 2018 by Umair Ahmed and Syed Moiz Hussain.
 *
 *  "Yune" is a framework for a Physically Based Renderer. It's aimed at young
 *  researchers trying to implement Physically Based Rendering techniques.
 *
 *  "Yune" is a free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  "Yune" is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 ******************************************************************************/

#ifndef CL_HEADERS_H
#define CL_HEADERS_H

#ifndef CL_MINIMUM_OPENCL_VERSION
#define CL_MINIMUM_OPENCL_VERSION 120
#endif // CL_MINIMUM_OPENCL_VERSION

#ifndef CL_TARGET_OPENCL_VERSION
#define CL_TARGET_OPENCL_VERSION 120
#endif // CL_TARGET_OPENCL_VERSION

#if defined(__APPLE__) || defined(__MACOSX)
    #include <OpenCL/opencl.h>
    #include <OpenGL/OpenGL.h>
    #define CL_GL_SHARING_EXT "cl_apple_gl_sharing"

#else
    #if defined (__linux)
        #include <GL/glx.h>
    #elif defined( __WIN32 )
        #include <windows.h>
    #endif // (__linux)

    #include <CL/cl.h>
    #include <CL/cl_gl.h>
    #define CL_GL_SHARING_EXT "cl_khr_gl_sharing"
#endif // if defined(__APPLE__) || defined(__MACOSX)

/* When using CL_MEM_ALLOC_HOST_PTR we don't need to worry about alignment. However when using the "USE_HOST_PTR" flag we have
 * to provide aligned data. We try to ensure aligned memory anyways in-case we want to measure performance difference between the two flags.
 */
struct Cam
{
    cl_float4 r1;
    cl_float4 r2;
    cl_float4 r3;
    cl_float4 r4;
    cl_float view_plane_dist;   // total 68 bytes
    cl_float pad[3];            // padding 12 bytes to reach 80 (next multiple of 16)
} __attribute__((aligned(16)));

struct Triangle
{
    cl_float4 v1;
    cl_float4 v2;
    cl_float4 v3;
    cl_float4 vn1;
    cl_float4 vn2;
    cl_float4 vn3;
    cl_int matID;       // total size till here = 100 bytes
    cl_float pad[3];    // padding 12 bytes - to make it 112 (next multiple of 16)
} __attribute__((aligned(16)));

struct Material
{
    cl_float4 emissive;
    cl_float4 diff;
    cl_float4 spec;
    cl_float ior;
    cl_float alpha_x;
    cl_float alpha_y;
    cl_short is_specular;
    cl_short is_transmissive;    // total 64 bytes.
} __attribute__((aligned(16)));


#endif // CL_HEADERS_H
