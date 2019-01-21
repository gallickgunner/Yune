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

#ifndef RENDERER_H
#define RENDERER_H

#include <Scene.h>
#include <CL_context.h>
#include <GL_context.h>
#include <FreeImage.h>
#include <string>
#include <vector>

namespace yune
{
    /** \brief Main Renderer class that does work on the GPU.
     *
     *  This class does the main work of rendering. It creates a \ref GL_context
     *  establishes OpenCL-GL shared context and sends kernels onto the GPU.
     */

    class Renderer
    {
        public:

            Renderer();                     /**< Default Constructor. */
            Renderer(Scene& render_scene);  /**< Overloaded Constructor taking \ref Scene object as input. */
            ~Renderer();                    /**< Default Destructor. */

            /** \brief A function to setup the \ref Renderer object.
             *
             * \param[in] render_scene  Pointer to a \ref Scene object.
             * \param[in] save_filename String representing the filename to use when saving images.
             * \param[in] ext           String representing the extension. Valid extensions are {png, jpg, jpe, jpg, bmp}
             *
             */
            void setup(Scene* render_scene);

            /** \brief Find the most efficient local workgroup size.
             */
            void getWorkGroupSizes();

            /** \brief Start the renderer by enqueuing "Pathtracer" kernel in a loop till the window closes.
             */
            void start();

            /** \brief Return the size of the Window.
             *
             * \param[out] win_width    The Width of the Window.
             * \param[out] win_height   The Height of the Window.
             *
             * Note that the window size can be small but cover the whole monitor if the fullscreen flag is set in setup().
             */
            void getWindowSize(int& win_width, int& win_height);

            /** \brief Return the size of the Image/Frambuffer we are rendering to
             *
             * \param[out] fb_width     The Width of the Image/Frambuffer.
             * \param[out] fb_height    The Height of the Image/Framebuffer
             *
             * This is for systems like MAC where the Framebuffer size (actual rendering resolution) is often different from the Window size.
             */
            void getFrameBufferSize(int& fb_width, int& fb_height);

            GL_context glfw_manager;      /**< A \ref GL_context object. */
            CL_context cl_manager;        /**< A \ref CL_context object. */
            Scene* render_scene;           /**< A \ref Scene object. Contains the Model data, lights and Camera. */

            private:
            void updateKernelArguments(GL_context::Options& opts, cl_int& reset, cl_uint seed, bool buffer_switch);
            void saveImage(unsigned long samples_taken, int time_passed, int fps);

            size_t gws[2];                      /**< Global workgroup size.*/
            size_t lws[2];                      /**< Local workgroup size.*/
            std::vector<cl_float> cam_data;     /**< A buffer containing Camera data for passing to the GPU.*/
            std::vector<cl_float> scene_data;   /**< A buffer containing Scene data for passing to the GPU.*/
    };
}
#endif // RENDERER_H
