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

#ifndef GLFWMANAGER_H
#define GLFWMANAGER_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <Gui.h>
#include <Eigen_typedefs.h>
#include <functional>
#include <atomic>

namespace yune
{
    /** \brief This class manages the OpenGL context, Window creation and other input related functions through GLFW.
     *         The release of OpenGL context and other resources is done in the desturctor.  This class is intended for
     *         1 time use only i.e. no more than one instance
     */
    class GL_context
    {
        public:
            GL_context();      /**< Default Constructor. */
            ~GL_context();     /**< Default Destructor. */

            /** \brief Creates a GLFW window encapsulating an OpenGL context.
             *
             * \param[in] width The Width in screen coordinates determining the size of the window or the initial framebuffer. This is passed on from the Renderer object.
             * \param[in] height The Height in screen coordinates determining the size of the window or the initial framebuffer. This is passed on from the Renderer object.
             *
             */
            void createWindow(int width, int height, bool is_fullscreen = false);

            /** \brief Setup OpenGL Framebuffer and RenderBuffer objects.
             *
             *  The OpenCL Image Object is created on top of this RBO which will then be used to draw the Image object data returned by the kernel on the window.
             */
            void setupGlBuffer();

            /** \brief Show the created Window.
             *
             *  Windows are created hidden initially. This functions is called just before entering the event processing loop to display the window.
             */
            void showWindow();

            /** \brief Set the callback function to be called when Camera is moved through keyboard/mouse.
             *
             */
            void setCameraUpdateCallback(std::function<void(const Vec4f&, float, float)>);

            GLFWwindow* window;                 /**< A GLFW Window pointer for rendering GUI and displaying image output from kernel. */
            int window_width;                   /**< Width of the window. */
            int window_height;                  /**< Height of the window. */
            int framebuffer_width;              /**< Width of the Framebuffer. */
            int framebuffer_height;             /**< Height of the Framebuffer. */

        private:
            friend class Renderer;
            static std::function<void(const Vec4f&, float, float)> cameraUpdateCallback;   /**< The function pointer to the Camera Update Callback function. */
            static Gui* gui;             /**< A Gui object for setting up some simple widgets. */
            GLFWmonitor* prim_monitor;  /**< GLFW monitor pointer storing primary monitor. */
            const GLFWvidmode* mode;    /**< GLFW Video mode pointer. */
            GLuint fbo_ID;              /**< OpenGL FrameBuffer Object ID */
            GLuint rbo_IDs[3];          /**< OpenGL RenderBuffer Object IDs for OpenCL read and write only Images. The third RBO is temporary for storing a copy of latest output image by kernel. */
            float old_cursor_x;         /**< Store the previous cursor X coordinate.*/
            float old_cursor_y;         /**< Store the previous cursor Y coordinate.*/
            bool space_flag;            /**< Store if the Space Key is in pressed state.*/
            bool is_fullscreen;         /**< A flag determining whether the window is fullscreen.*/

            /** \name GLFW Callbacks
             *  Refer to the GLFW docs for details on the callback function syntax.
             */
            ///@{
            static void errorCallback(int error, const char* msg);
            static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
            static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
            static void cursorPosCallback(GLFWwindow* window, double new_cursor_x, double new_cursor_y);
            static void charCallback(GLFWwindow *w, unsigned int codepoint);
            static void dropCallback(GLFWwindow *w, int count, const char **filenames);
            static void scrollCallback(GLFWwindow *w, double x, double y);
            static void framebufferSizeCallback(GLFWwindow* window, int width, int height);
            ///@}
    };
}

#endif // GLFWMANAGER_H
