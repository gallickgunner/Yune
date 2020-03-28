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

#ifndef GLFWMANAGER_H
#define GLFWMANAGER_H

#include "glad/glad.h"
#include "GLFW/glfw3.h"
#include "glm/vec4.hpp"
#include <functional>

namespace yune
{
    /** \brief This class manages the OpenGL context, Window creation and other input related functions through GLFW.
     *         The release of OpenGL context and other resources is done in the desturctor.  This class is intended for
     *         1 time use only i.e. no more than one instance
     */
    class GlfwManager
    {
        public:
            /** \brief Overloaded Constructor.
             *
             * \param[in] window_width The width of the window.
             * \param[in] window_height The height of the window.
             */
            GlfwManager(int window_width = 1024, int window_height = 768);
            ~GlfwManager();     /**< Default Destructor. */

            /** \brief Setup OpenGL Framebuffer and RenderBuffer objects.
             *
             * The OpenCL Image Object is created on top of this RBO which will then be used to draw the Image object data returned by the kernel on the window.
             * \return True or false depending on if the Buffers are created successfully
             */
            bool setupGlBuffer();

            /** \brief Show the created Window.
             *
             *  Windows are created hidden initially. This functions is called just before entering the event processing loop to display the window.
             */
            void showWindow();

            /** \brief Set the callback function to be called when Camera is moved through keyboard/mouse.
             */
            void setCameraUpdateCallback(std::function<void(const glm::vec4&, float, float)>);

            /** \brief Set the callback function to set messages shown by GUI incase of any event
             */
            void setGuiMessageCb(std::function<void(const std::string&, const std::string&, const std::string&)>);

            GLFWwindow* window;             /**< A GLFW Window pointer for rendering GUI and displaying image output from kernel. */
            int window_width;               /**< Width of the window. */
            int window_height;              /**< Height of the window. */
            int framebuffer_width;          /**< Width of the Framebuffer. */
            int framebuffer_height;         /**< Height of the Framebuffer. */
            GLFWmonitor* prim_monitor;      /**< GLFW monitor pointer storing primary monitor. */
            const GLFWvidmode* mode;        /**< GLFW Video mode pointer. */

        private:
            friend class RendererCore;

            void initImGui();

            /** \brief Creates a GLFW window encapsulating an OpenGL context.
             *
             * \param[in] width The Width in screen coordinates determining the size of the window or the initial framebuffer. This is passed on from the Renderer object.
             * \param[in] height The Height in screen coordinates determining the size of the window or the initial framebuffer. This is passed on from the Renderer object.
             *
             */
            void createWindow(int width, int height);

            static std::function<void(const glm::vec4&, float, float)> cameraUpdateCallback;   /**< The function pointer to the Camera Update Callback function. */
            static std::function<void(const std::string&, const std::string&, const std::string&)> setMessageCb;   /**< The function pointer to the RendererGUI message callback function. */

            GLuint fbo_ID;              /**< OpenGL FrameBuffer Object ID */
            GLuint rbo_IDs[4];          /**< OpenGL RenderBuffer Object IDs for OpenCL read and write only Images and post-processing. The fourth RBO is temporary for storing a copy of latest output image by kernel. */
            float old_cursor_x;         /**< Store the previous cursor X coordinate.*/
            float old_cursor_y;         /**< Store the previous cursor Y coordinate.*/
            bool space_flag;            /**< Store if the Space Key is in pressed state.*/

            /** \name GLFW Callbacks
             *  Refer to the GLFW docs for details on the callback function syntax.
             */
            ///@{
            static void errorCallback(int error, const char* msg);
            static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
            static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
            static void cursorPosCallback(GLFWwindow* window, double new_cursor_x, double new_cursor_y);
            static void framebufferSizeCallback(GLFWwindow* window, int width, int height);
            ///@}
    };
}

#endif // GLFWMANAGER_H
