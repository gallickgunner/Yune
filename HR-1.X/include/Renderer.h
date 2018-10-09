/******************************************************************************
 *  This file is part of "Helion-Render".
 *
 *  Copyright (C) 2018 by Umair Ahmed and Syed Moiz Hussain.
 *
 *  "Helion-Render" is a Physically based Renderer using Bi-Directional Path Tracing.
 *  Right now the renderer only  works for devices that support OpenCL and OpenGL.
 *
 *  "Helion-Render" is a free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  "Helion-Render" is distributed in the hope that it will be useful,
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

namespace helion_render
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
             * \param[in] glfw_manager  Pointer to a \ref GL_context object.
             * \param[in] cl_manager    Pointer to a \ref CL_context object.
             * \param[in] render_scene  Pointer to a \ref Scene object.
             * \param[in] save_filename String representing the filename to use when saving images.
             * \param[in] ext           String representing the extension. Valid extensions are {png, jpg, jpe, jpg, bmp}
             *
             */
            void setup(GL_context* glfw_manager, CL_context* cl_manager, Scene* render_scene, std::string save_filename = "", std::string ext = ".png");

            /** \brief Find the most efficient local workgroup size.
             *
             * \param[out] global_workgroup_size[2] A size_t array defining the total amount of work items in each of the 2 dimensions.
             * \param[out] local_workgroup_size[2]  A size_t array defining the total amount of work items in each of the 2 dimension within a workgroup.
             *
             */
            void getWorkGroupSizes(size_t global_workgroup_size[2], size_t local_workgroup_size[2]);

            /** \brief Start the renderer by enqueuing "Pathtracer" kernel in a loop till the window closes.
             *
             * \param[in] global_workgroup_size[2]  A size_t array defining the total amount of work items in each of the 2 dimensions.
             * \param[in] local_workgroup_size[2]   A size_t array defining the total amount of work items in each of the 2 dimension within a workgroup.
             *
             *  This overload is for users who want to manually provide the size of workgroup and total work items.
             *  However, the renderer does calculate the best sizes for the selected device.
             */
            void start(size_t global_workgroup_size[2], size_t local_workgroup_size[2]);

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

            GL_context* glfw_manager;      /**< A \ref GL_context object. */
            CL_context* cl_manager;        /**< A \ref CL_context object. */
            Scene* render_scene;           /**< A \ref Scene object. Contains the Model data, lights and Camera. */

            private:
            void updateKernelArguments(GL_context::Options& opts, cl_int& reset, cl_uint seed, bool buffer_switch);
            void saveImage(unsigned long samples_taken);

            std::vector<cl_float> cam_data;     /**< A buffer containing Camera data for passing to the GPU.*/
            std::vector<cl_float> scene_data;   /**< A buffer containing Scene data for passing to the GPU.*/
            std::string save_filename;          /**< The filename to use when saving images. The number of samples per pixel is automatically appended. */
            std::string ext;                    /**< The extension to use. Valid extensions are {png, jpeg, jpg, jpe, bmp} */
    };
}
#endif // RENDERER_H
