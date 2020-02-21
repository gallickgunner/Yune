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

#include <GL_context.h>
#include <RuntimeError.h>
#include <nanogui/nanogui.h>

#include <iostream>
#include <functional>
#include <limits>
#include <vector>

namespace yune
{
    Gui* GL_context::gui = new Gui;
    std::function<void(const Vec4f&, float, float)> GL_context::cameraUpdateCallback;

    GL_context::GL_context()
    {
        //ctor
        glfwSetErrorCallback(errorCallback);
        if(!glfwInit())
            throw RuntimeError("GLFW failed to initialize.");
        window = NULL;
        space_flag = false;
    }

    GL_context::~GL_context()
    {
        //Release OpenGL RBOs and FBO
        if(window)
        {
            glDeleteRenderbuffers(3, rbo_IDs);
            glDeleteFramebuffers(1, &fbo_ID);
        }
        delete gui;

        //Release GLFW resources.
        if(window)
            glfwDestroyWindow(window);
        glfwTerminate();
    }

    void GL_context::createWindow(int window_width, int window_height, bool is_fullscreen)
    {
        // get Primary Monitor and default Video Mode
        prim_monitor = glfwGetPrimaryMonitor();
        mode = glfwGetVideoMode(prim_monitor);

        // Context Hints.
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        //FrameBuffer Hints.
        glfwWindowHint(GLFW_RED_BITS, 8);
        glfwWindowHint(GLFW_GREEN_BITS, 8);
        glfwWindowHint(GLFW_BLUE_BITS, 8);
        glfwWindowHint(GLFW_ALPHA_BITS, 8);
        glfwWindowHint(GLFW_STENCIL_BITS, 8);
        glfwWindowHint(GLFW_DEPTH_BITS, 24);

        //Window Hints
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);

        // If fullscreen flag set, make a borderless fullscreen window.
        this->is_fullscreen = is_fullscreen;
        if(is_fullscreen)
        {
            this->window_width = mode->width;
            this->window_height = mode->height;
            glfwWindowHint(GLFW_RED_BITS, mode->redBits);
            glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
            glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
            glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
            glfwWindowHint(GLFW_AUTO_ICONIFY, true);
            window = glfwCreateWindow(mode->width, mode->height, "Yune", prim_monitor, NULL);
        }
        else
        {
            this->window_width = window_width;
            this->window_height = window_height;
            window = glfwCreateWindow(window_width, window_height, "Yune", NULL, NULL);
        }

        if(!window)
        {
            glfwTerminate();
            throw RuntimeError("Window creation failed..");
        }

        glfwMakeContextCurrent(window);
        if( !gladLoadGLLoader( (GLADloadproc) glfwGetProcAddress) )
            throw RuntimeError("Couldn't Initialize GLAD..");
        if(!GLAD_GL_VERSION_3_3)
            throw RuntimeError("OpenGL version 3.3 not supported");

        glGetError();
        glfwSetWindowUserPointer(window, this);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        glfwSetKeyCallback(window, keyCallback);
        glfwSetMouseButtonCallback(window, mouseButtonCallback);
        glfwSetCursorPosCallback(window, cursorPosCallback);
        glfwSetCharCallback(window, charCallback);
        glfwSetScrollCallback(window, scrollCallback);
        glfwSetDropCallback(window, dropCallback);
        glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

        gui->setupWidgets(window);
        glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
        glViewport(0,0, framebuffer_width, framebuffer_height);
        glfwIconifyWindow(window);
        glfwSwapInterval(0);
        glfwPollEvents();

    }

    void GL_context::setupGlBuffer()
    {

        glGenFramebuffers(1,&fbo_ID);
        glBindFramebuffer(GL_FRAMEBUFFER,fbo_ID);

        glGenRenderbuffers(3, rbo_IDs);

        glBindRenderbuffer(GL_RENDERBUFFER, rbo_IDs[0]);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA32F, framebuffer_width, framebuffer_height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbo_IDs[0]);

        glBindRenderbuffer(GL_RENDERBUFFER, rbo_IDs[1]);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA32F, framebuffer_width, framebuffer_height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_RENDERBUFFER, rbo_IDs[1]);

        glBindRenderbuffer(GL_RENDERBUFFER, rbo_IDs[2]);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA32F, framebuffer_width, framebuffer_height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_RENDERBUFFER, rbo_IDs[2]);

        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

        if(status != GL_FRAMEBUFFER_COMPLETE)
        {
            if(status == GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT)
                throw RuntimeError("Framebuffer not complete. Error code: GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT");
            else if(status == GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT)
                throw RuntimeError("Framebuffer not complete. Error code: GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT");
            else if(status == GL_FRAMEBUFFER_UNDEFINED)
                throw RuntimeError("Framebuffer not complete. Error code: GL_FRAMEBUFFER_UNDEFINED");
            else if(status == GL_FRAMEBUFFER_UNSUPPORTED)
                throw RuntimeError("Framebuffer not complete. Error code: GL_FRAMEBUFFER_UNSUPPORTED");
            else if(status == GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER)
                throw RuntimeError("Framebuffer not complete. Error code: GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER");
            else if(status == GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER)
                throw RuntimeError("Framebuffer not complete. Error code: GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER");
            else
                throw RuntimeError("Framebuffer not complete.");
        }

        glClearColor(0.f, 0.f, 0.f, 1.0f);
        glDrawBuffer(GL_COLOR_ATTACHMENT0);
        glClear(GL_COLOR_BUFFER_BIT);

        glDrawBuffer(GL_COLOR_ATTACHMENT1);
        glClear(GL_COLOR_BUFFER_BIT);

        glDrawBuffer(GL_COLOR_ATTACHMENT2);
        glClear(GL_COLOR_BUFFER_BIT);
    }


    void GL_context::showWindow()
    {
        glfwRestoreWindow(window);
        glfwFocusWindow(window);
    }

    void GL_context::errorCallback(int error, const char* msg)
    {
        std::cout << msg << std::endl;
    }

    void GL_context::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
    {
        GL_context* ptr = (GL_context*) glfwGetWindowUserPointer(window);

        if(key == GLFW_KEY_UP &&  (action == GLFW_PRESS || action == GLFW_REPEAT) )
          GL_context::cameraUpdateCallback(Vec4f(0,0,1,0), 0, 0);

        if(key == GLFW_KEY_DOWN &&  (action == GLFW_PRESS || action == GLFW_REPEAT) )
          cameraUpdateCallback(Vec4f(0,0, -1,0), 0, 0);

        if(key == GLFW_KEY_LEFT &&  (action == GLFW_PRESS || action == GLFW_REPEAT) )
           cameraUpdateCallback(Vec4f(-1,0,0,0), 0, 0);

        if(key == GLFW_KEY_RIGHT &&  (action == GLFW_PRESS || action == GLFW_REPEAT) )
            cameraUpdateCallback(Vec4f(1,0,0,0), 0, 0);

        if(key == GLFW_KEY_SPACE &&  action == GLFW_PRESS )
            ptr->space_flag = true;
        if(key == GLFW_KEY_SPACE &&  action == GLFW_RELEASE )
            ptr->space_flag = false;

        gui->screen->keyCallbackEvent(key, scancode, action, mods);
    }

    void GL_context::cursorPosCallback(GLFWwindow* window, double new_cursor_x, double new_cursor_y)
    {
        GL_context* ptr = (GL_context*) glfwGetWindowUserPointer(window);
        if(ptr->space_flag)
        {
            float delta_x, delta_y;
            float x_rad = 0.06, y_rad = 0.06;

            delta_x = new_cursor_x - ptr->old_cursor_x;
            delta_y = new_cursor_y - ptr->old_cursor_y;

            if(delta_x < 1 && delta_x > -1)
                y_rad = 0;
            else if (delta_x > 0)
                y_rad *= -1.0;

            if(delta_y < 1 && delta_y > -1)
                x_rad = 0;
            else if(delta_y > 0)
                x_rad *= -1.0;
            cameraUpdateCallback(Vec4f(0,0,0,0), x_rad, y_rad);
            ptr->old_cursor_x = new_cursor_x;
            ptr->old_cursor_y = new_cursor_y;
        }
        else
        {
            ptr->old_cursor_x = new_cursor_x;
            ptr->old_cursor_y = new_cursor_y;
        }
        gui->screen->cursorPosCallbackEvent(new_cursor_x, new_cursor_y);

    }

    void GL_context::mouseButtonCallback(GLFWwindow* window, int button, int action, int modifiers)
    {
        gui->screen->mouseButtonCallbackEvent(button, action, modifiers);
    }

    void GL_context::charCallback(GLFWwindow *w, unsigned int codepoint)
    {
        gui->screen->charCallbackEvent(codepoint);
    }

    void GL_context::dropCallback(GLFWwindow *w, int count, const char **filenames)
    {
        gui->screen->dropCallbackEvent(count, filenames);
    }

    void GL_context::scrollCallback(GLFWwindow *w, double x, double y)
    {
        gui->screen->scrollCallbackEvent(x, y);
    }

    void GL_context::framebufferSizeCallback(GLFWwindow* window, int width, int height)
    {
        gui->screen->resizeCallbackEvent(width, height);
    }

    void GL_context::setCameraUpdateCallback(std::function<void(const Vec4f&, float, float)> cb)
    {
        GL_context::cameraUpdateCallback = cb;
    }

}
