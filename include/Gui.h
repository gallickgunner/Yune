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

#ifndef GUI_H
#define GUI_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <nanogui/nanogui.h>

namespace yune
{
    class Gui
    {
        private:
            /** \brief A structure to hold profiling values for kernels.
             */
            struct Profiler
            {
                nanogui::IntBox<int>* fps;              /**< Text box to display FPS. This is **not** averaged FPS. */
                nanogui::FloatBox<float>* mspf_avg;     /**< Text box to display milliseconds per frame. This is averaged over 1 second. */
                nanogui::FloatBox<float>* msrk_avg;     /**< Text box to display render kernel execution time. This is averaged over 1 second. */
                nanogui::FloatBox<float>* msppk_avg;    /**< Text box to display Post-processing kernel execution time. This is averaged over 1 second. */
                nanogui::IntBox<int>* render_time;      /**< Text box to display render time. */
                nanogui::IntBox<int>* tot_samples;      /**< Text box to display total samples */
            };

        public:
            Gui();      /**< Default Constructor. */
            ~Gui();     /**< Default Destructor. */

            void setupWidgets(GLFWwindow* window);       /**< A simple function that sets up widgets. */
            Profiler profiler;                      /**< A Profiler structure variable. */
            static nanogui::Screen* screen;         /**< A pointer to nanogui::Screen. It's a wrapper around a GLFW window. Refer to nanogui docs for details. */
            nanogui::Window* panel;                 /**< A pointer to nanogui::Window. Used as a base panel for widgets. Refer to nanogui docs for details. */
            nanogui::Window* profiler_window;       /**< A pointer to nanogui::Window. Used as a base panel for widgets. Refer to nanogui docs for details. */
            nanogui::Theme* panel_theme;            /**< A pointer to nanogui::Theme */
            nanogui::TextBox* save_img_name;        /**< Text box to store the filename for saved images. */
            nanogui::TextBox* save_hdr;             /**< Text box to store the extension of the image saved. */
            nanogui::IntBox<int>* save_img_samples; /**< Int box to store the number of samples at which to save image. */
            nanogui::CheckBox* gi_check;            /**< Check box to store whether GI is enabled/disabled. */
            nanogui::CheckBox* hdr_check;           /**< Check box to store whether to save image in HDR format (EXR) or LDR (jpeg). */
            bool start_btn;                         /**< Check if start button was pressed. Accessed by \ref Renderer.*/
            bool stop_btn;                          /**< Check if stop button was pressed. Accessed by \ref Renderer.*/
            bool save_img_btn;                      /**< Check if the save button was pressed. Accessed by \ref Renderer.*/
    };
}
#endif // GUI_H
