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

#include "GlfwManager.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <exception>
#include <iostream>
#include <functional>
#include <limits>
#include <vector>
#include <string>

namespace yune
{
    std::function<void(const glm::vec4&, float, float)> GlfwManager::cameraUpdateCallback;
    std::function<void(const std::string&, const std::string&, const std::string&)> GlfwManager::setMessageCb;

    GlfwManager::GlfwManager(int window_width, int window_height)
    {
        //ctor
        glfwSetErrorCallback(errorCallback);
        if(!glfwInit())
            throw std::runtime_error("GLFW failed to initialize.");
        window = NULL;
        space_flag = false;
        createWindow(window_width, window_height);
        initImGui();
    }

    GlfwManager::~GlfwManager()
    {
        //Release OpenGL RBOs and FBO
        if(window)
        {
            glDeleteRenderbuffers(3, rbo_IDs);
            glDeleteFramebuffers(1, &fbo_ID);
        }

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        if(window)
            glfwDestroyWindow(window);
        glfwTerminate();
    }

    void GlfwManager::initImGui()
    {
        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

         // Setup Dear ImGui style
        ImGui::StyleColorsDark();

        // Setup Platform/Renderer bindings
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 330");
    }

    void GlfwManager::createWindow(int window_width, int window_height)
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
        glfwWindowHint(GLFW_RED_BITS, mode->redBits);
        glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
        glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
        glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
        glfwWindowHint(GLFW_ALPHA_BITS, 8);
        glfwWindowHint(GLFW_STENCIL_BITS, 8);
        glfwWindowHint(GLFW_DEPTH_BITS, 24);


        //Window Hints
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);

        this->window_width = window_width;
        this->window_height = window_height;
        window = glfwCreateWindow(window_width, window_height, "Yune", NULL, NULL);

        if(!window)
        {
            glfwTerminate();
            throw std::runtime_error("Window creation failed..");
        }

        glfwMakeContextCurrent(window);
        if( !gladLoadGLLoader( (GLADloadproc) glfwGetProcAddress) )
            throw std::runtime_error("Couldn't Initialize GLAD..");
        if(!GLAD_GL_VERSION_3_3)
            throw std::runtime_error("OpenGL version 3.3 not supported");

        glGetError();
        glfwSetWindowUserPointer(window, this);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        glfwSetKeyCallback(window, keyCallback);
        glfwSetCursorPosCallback(window, cursorPosCallback);
        glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

        glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
        glViewport(0,0, framebuffer_width, framebuffer_height);
        glfwIconifyWindow(window);
        glClear(GL_COLOR_BUFFER_BIT);
        glfwSwapInterval(0);
        glfwPollEvents();
    }

    bool GlfwManager::setupGlBuffer()
    {
        //Delete any previous RenderBuffer/FrameBuffer. Note that delete calls silently ignore any unused names and 0's
        glDeleteRenderbuffers(4, rbo_IDs);
        glDeleteFramebuffers(1, &fbo_ID);

        glGenFramebuffers(1,&fbo_ID);
        glBindFramebuffer(GL_FRAMEBUFFER,fbo_ID);

        glGenRenderbuffers(4, rbo_IDs);

        glBindRenderbuffer(GL_RENDERBUFFER, rbo_IDs[0]);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA32F, framebuffer_width, framebuffer_height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbo_IDs[0]);

        glBindRenderbuffer(GL_RENDERBUFFER, rbo_IDs[1]);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA32F, framebuffer_width, framebuffer_height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_RENDERBUFFER, rbo_IDs[1]);

        glBindRenderbuffer(GL_RENDERBUFFER, rbo_IDs[2]);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA32F, framebuffer_width, framebuffer_height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_RENDERBUFFER, rbo_IDs[2]);

        glBindRenderbuffer(GL_RENDERBUFFER, rbo_IDs[3]);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA32F, framebuffer_width, framebuffer_height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_RENDERBUFFER, rbo_IDs[3]);

        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

        if(status != GL_FRAMEBUFFER_COMPLETE)
        {
            if(status == GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT)
                setMessageCb("Framebuffer not complete. Error code: GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT", "FrameBuffer Incomplete!", "");
            else if(status == GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT)
                setMessageCb("Framebuffer not complete. Error code: GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT", "FrameBuffer Incomplete!", "");
            else if(status == GL_FRAMEBUFFER_UNDEFINED)
                setMessageCb("Framebuffer not complete. Error code: GL_FRAMEBUFFER_UNDEFINED", "FrameBuffer Incomplete!", "");
            else if(status == GL_FRAMEBUFFER_UNSUPPORTED)
                setMessageCb("Framebuffer not complete. Error code: GL_FRAMEBUFFER_UNSUPPORTED", "FrameBuffer Incomplete!", "");
            else if(status == GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER)
                setMessageCb("Framebuffer not complete. Error code: GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER", "FrameBuffer Incomplete!", "");
            else if(status == GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER)
                setMessageCb("Framebuffer not complete. Error code: GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER", "FrameBuffer Incomplete!", "");
            else
                setMessageCb("Framebuffer not complete. Error code: " + std::to_string((int)status), "FrameBuffer Incomplete!", "");
            return false;
        }

        glClearColor(0.f, 0.f, 0.f, 1.0f);
        glDrawBuffer(GL_COLOR_ATTACHMENT0);
        glClear(GL_COLOR_BUFFER_BIT);

        glDrawBuffer(GL_COLOR_ATTACHMENT1);
        glClear(GL_COLOR_BUFFER_BIT);

        glDrawBuffer(GL_COLOR_ATTACHMENT2);
        glClear(GL_COLOR_BUFFER_BIT);

        glDrawBuffer(GL_COLOR_ATTACHMENT3);
        glClear(GL_COLOR_BUFFER_BIT);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        return true;
    }

    void GlfwManager::showWindow()
    {
        glfwRestoreWindow(window);
        glfwFocusWindow(window);
    }

    void GlfwManager::errorCallback(int error, const char* msg)
    {
        std::cout << msg << std::endl;
    }

    void GlfwManager::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
    {
        if(ImGui::GetIO().WantCaptureKeyboard)
            return;
        GlfwManager* ptr = (GlfwManager*) glfwGetWindowUserPointer(window);

        if(key == GLFW_KEY_UP &&  (action == GLFW_PRESS || action == GLFW_REPEAT) )
          GlfwManager::cameraUpdateCallback(glm::vec4(0,0,1,0), 0, 0);

        if(key == GLFW_KEY_DOWN &&  (action == GLFW_PRESS || action == GLFW_REPEAT) )
          cameraUpdateCallback(glm::vec4(0,0, -1,0), 0, 0);

        if(key == GLFW_KEY_LEFT &&  (action == GLFW_PRESS || action == GLFW_REPEAT) )
           cameraUpdateCallback(glm::vec4(-1,0,0,0), 0, 0);

        if(key == GLFW_KEY_RIGHT &&  (action == GLFW_PRESS || action == GLFW_REPEAT) )
            cameraUpdateCallback(glm::vec4(1,0,0,0), 0, 0);

        if(key == GLFW_KEY_HOME &&  (action == GLFW_PRESS || action == GLFW_REPEAT) )
          GlfwManager::cameraUpdateCallback(glm::vec4(0,1,0,0), 0, 0);

        if(key == GLFW_KEY_END &&  (action == GLFW_PRESS || action == GLFW_REPEAT) )
          cameraUpdateCallback(glm::vec4(0,-1, 0,0), 0, 0);

        if(key == GLFW_KEY_SPACE &&  action == GLFW_PRESS )
            ptr->space_flag = true;
        if(key == GLFW_KEY_SPACE &&  action == GLFW_RELEASE )
            ptr->space_flag = false;

    }

    void GlfwManager::cursorPosCallback(GLFWwindow* window, double new_cursor_x, double new_cursor_y)
    {
        GlfwManager* ptr = (GlfwManager*) glfwGetWindowUserPointer(window);
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
            cameraUpdateCallback(glm::vec4(0,0,0,0), x_rad, y_rad);
            ptr->old_cursor_x = new_cursor_x;
            ptr->old_cursor_y = new_cursor_y;
        }
        else
        {
            ptr->old_cursor_x = new_cursor_x;
            ptr->old_cursor_y = new_cursor_y;
        }
    }

    void GlfwManager::framebufferSizeCallback(GLFWwindow* window, int width, int height)
    {
        GlfwManager* ptr = (GlfwManager*) glfwGetWindowUserPointer(window);
        glViewport(0, 0, width, height);
        ptr->framebuffer_width = width;
        ptr->framebuffer_height = height;
    }

    void GlfwManager::setCameraUpdateCallback(std::function<void(const glm::vec4&, float, float)> cb)
    {
        GlfwManager::cameraUpdateCallback = cb;
    }

    void GlfwManager::setGuiMessageCb(std::function<void(const std::string&, const std::string&, const std::string&)> cb)
    {
        GlfwManager::setMessageCb = cb;
    }
}
