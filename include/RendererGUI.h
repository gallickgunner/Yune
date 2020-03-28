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

#ifndef RENDERERGUI_H
#define RENDERERGUI_H

#define IMGUI_IMPL_OPENGL_LOADER_GLAD

#include "GlfwManager.h"
#include "CLManager.h"
#include "RendererCore.h"
#include "glm/vec4.hpp"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "ImGuiFileBrowser.h"

namespace yune
{
    /** \brief This class is body of the renderer. It manages and connects different parts of the renderer with each other.
     *         It contains instances to CLManager, GlfwManager and RendererCore which is the core part dealing with
     *         enqueuing kernels.
     */
    class RendererGUI
    {
        public:
            RendererGUI(int window_width = 1024, int window_height = 768);
            ~RendererGUI();
            void run();

        private:
            struct WindowSize
            {
                int width;
                int height;
            };

            bool setup();
            void startFrame();
            void renderFrame();
            bool showMenu();
            void showBenchmarkWindow(unsigned long time_passed);
            void showSceneInfo();
            void showMiscSettings();
            void showMessageBox(std::string title, std::string msg, std::string log = "");
            void showHelpMarker(std::string desc);
            void renderExtBox();

            static void setMessage(const std::string& msg, const std::string& title, const std::string& log);
            static std::string mb_msg, mb_title, mb_log;

            GlfwManager glfw_manager;
            CLManager cl_manager;
            RendererCore renderer;
            std::vector<WindowSize> supported_sizes;
            std::vector<std::string> supported_exts;
            imgui_addons::ImGuiFileBrowser file_dialog;

            char input_fn[256];
            int benchmark_wheight, bvh_bins, selected_size;
            bool benchmark_shown, scene_info_shown, misc_settings_shown, renderer_start;
            bool is_fullscreen, update_vertex_buffer, update_mat_buffer, update_image_buffer, update_bvh_buffer, load_bvh, gi_check, cap_fps, do_postproc;
    };
}
#endif // RENDERERGUI_H
