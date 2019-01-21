/******************************************************************************
 *  This file is part of "Yune".
 *
 *  Copyright (C) 2018 by Umair Ahmed and Syed Moiz Hussain.
 *
 *  "Yune" is a Physically based Renderer using Bi-Directional Path Tracing.
 *  Right now the renderer only  works for devices that support OpenCL and OpenGL.
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
    #include <OpenGL/OpennGL.h>
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

#endif // CL_HEADERS_H
