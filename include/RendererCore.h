/******************************************************************************
 *  This file is part of Yune".
 *
 *  Copyright (C) 2018 by Umair Ahmed and Syed Moiz Hussain.
 *
 *  "Yune" is a personal project and an educational raytracer/pathtracer. It's aimed at young
 *  researchers trying to implement raytracing/pathtracing but don't want to mess with boilerplate code.
 *  It provides all basic functionality to open a window, save images, etc. Since it's GPU based, you only need to modify
 *  the kernel file and see your program in action. For details, check <https://github.com/gallickgunner/Yune>
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

#ifndef RENDERERCORE_H
#define RENDERERCORE_H

#include "Scene.h"
#include "CLManager.h"
#include "GlfwManager.h"
#include "glm/vec2.hpp"
#include <chrono>
#include <random>
#include <limits>
#include <string>
#include <vector>

namespace yune
{
    /** \brief This is the logical counterpart to RendererGUI. Mainly responsible for setting kernel arguments, enqueuing kernels and blitting the resulting image to default framebuffer.
     */
    class RendererCore
    {
        public:
            RendererCore(CLManager& cl_manager, GlfwManager& glfw_manager);
            ~RendererCore();    /**< Default Destructor. */

            /** \brief A function to setup the \ref RendererCore object.
             *
             * \param[in] update_image_buffer   Whether to create CL memory object for image
             * \param[in] update_vertex_buffer  Whether to create CL memory object for vertex data
             * \param[in] update_mat_buffer     Whether to create CL memory object for material data
             * \param[in] update_bvh_buffer     Whether to create CL memory object for bvh
             * \param[in] do_postproc           If post-processing kernel is loaded or not.
             *
             */
            bool setup(bool& update_image_buffer, bool& update_scene_buffer, bool& update_mat_buffer, bool& update_bvh_buffer, bool do_postproc);

            /** \brief Enqueues kernel in blocks.
             *  This function enqueues kernels in blocks. After one whole frame is processed. Post processing kernel is enqueued if present.
             */
            bool enqueueKernels(bool new_gi_check, bool cap_fps);

            /** \brief Blit the last frame to the default framebuffer
             */
            void render();

            /** \brief Set the callback function to set messages shown by GUI incase of any event
             */
            void setGuiMessageCb(std::function<void(const std::string&, const std::string&, const std::string&)>);

            bool loadScene(std::string path, std::string fn);
            void updateKernelWGSize(bool reset = false);
            bool reloadMatFile();
            void resetValues();
            void stop();

            Scene render_scene;
            std::string save_fn, save_ext, save_samples_fn, save_samples_ext;
            bool save_pending, new_gi_check;
            unsigned long samples_taken, save_at_samples, time_passed;
            float ms_per_rk, ms_per_ppk, mspf_avg;
            int fps;

            glm::ivec2 blocks;
            size_t rk_gws[2];    /**< Global workgroup size for Rendeirng Kernel.*/
            size_t ppk_gws[2];   /**< Global workgroup size for Post-processing Kernel.*/
            size_t rk_lws[2];    /**< Local workgroup size for Rendering Kernel.*/
            size_t ppk_lws[2];   /**< Local workgroup size for Post-processing Kernel.*/

            private:
            void loadOptions();
            void updateRenderKernelArgs(bool new_gi_check, cl_uint seed);
            void updatePostProcessingKernelArgs();
            bool saveImage(std::string save_fn, std::string save_ext);
            void endFrame();

            static std::function<void(const std::string&, const std::string&, const std::string&)> setMessageCb;   /**< The function pointer to the RendererGUI message callback function. */

            CLManager& cl_manager;       /**< A \ref CLManager object. */
            GlfwManager& glfw_manager;    /**< A \ref GlfwManager object. */
            std::mt19937 mt_engine;
            std::uniform_int_distribution<unsigned int> dist;

            bool buffer_switch, gi_check, rk_enqueued, ppk_enqueued, do_postproc, render_nextframe;
            cl_int reset, rk_status, ppk_status;
            cl_uint seed;
            cl_event rk_event, ppk_event;
            int curr_block, frame_count;
            float skip_ticks, sum_mspf, mspf_uncapped_avg;
            double exec_time_rk, exec_time_ppk, last_time, start_time;
            unsigned int mt_seed;
            Cam cam_data;        /**< A Cam structure containing Camera data for passing to the GPU. A similar structure resides on GPU.*/
    };
}
#endif // RENDERERCORE_H
