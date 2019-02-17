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
    nanogui::Screen* GL_context::screen = NULL;
    std::function<void(const Vec4f&, float, float)> GL_context::cameraUpdateCallback;

    GL_context::GL_context()
    {
        glfwSetErrorCallback(errorCallback);
        if(!glfwInit())
            throw RuntimeError("GLFW failed to initialize.");
        window = NULL;
        space_flag = false;
        options.start = false;
        options.stop = false;
        options.save_img = false;
        options.gi_check = 1;
        options.save_img_samples =0;

        //ctor
    }

    GL_context::~GL_context()
    {
        //Release OpenGL RBOs and FBO
        if(window)
        {
            glDeleteRenderbuffers(3, rbo_IDs);
            glDeleteFramebuffers(1, &fbo_ID);
        }

        // Release Nanogui's Screen. Remember we passed **false** for mShutDownGLFWonDestruct while creating the screen.
        if(screen)
            delete screen;

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
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

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

            window = glfwCreateWindow(mode->width, mode->height, "Yune", prim_monitor, NULL);
            //glfwIconifyWindow(window);
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

        glfwSetWindowUserPointer(window, this);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        glfwSetKeyCallback(window, keyCallback);
        glfwSetMouseButtonCallback(window, mouseButtonCallback);
        glfwSetCursorPosCallback(window, cursorPosCallback);
        glfwSetCharCallback(window, charCallback);
        glfwSetScrollCallback(window, scrollCallback);
        glfwSetDropCallback(window, dropCallback);
        glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
        setupWidgets();
        glViewport(0,0, framebuffer_width, framebuffer_height);
        glfwSwapInterval(0);
        glfwPollEvents();

    }

    void GL_context::setupWidgets()
    {

        screen = new nanogui::Screen();
        screen->initialize(window, false);

        //Override nanogui's pixel ratio settings. We want to have a 1 to 1 correspondance with the device pixel, hence we may need to shrink window size.
        glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
        glfwGetWindowSize(window, &window_width, &window_height);
        if(window_width != framebuffer_width)
        {
            double pixel_ratio = (float)framebuffer_width/window_width;
            window_width = window_width/pixel_ratio;
            window_height = window_height/pixel_ratio;
            glfwSetWindowSize(window, window_width, window_height);
            glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
        }

        if(is_fullscreen)
            glfwIconifyWindow(window);

        //We don't need to save pointers to nanogui's child widgets such as labels and buttons. They get de-allocated when parent window destroys.

        //Set Options Panel
        panel = new nanogui::Window(screen, "Menu");
        panel->setPosition(Eigen::Vector2i(0, 0));

        nanogui::GroupLayout* panel_layout = new nanogui::GroupLayout();
        panel_layout->setMargin(5);
        panel_layout->setSpacing(1);
        panel->setLayout(panel_layout);

        //Set custom theme, transparency etc.
        panel_theme = new nanogui::Theme(screen->nvgContext());
        panel_theme->mWindowFillFocused = nanogui::Color(73, 170);
        panel_theme->mWindowFillUnfocused = nanogui::Color(70, 170);
        panel_theme->mWindowCornerRadius = 8;
        panel_theme->mWindowPopup = nanogui::Color(73, 190);
        panel_theme->mButtonGradientTopUnfocused = nanogui::Color(73, 145);
        panel_theme->mButtonGradientBotUnfocused = nanogui::Color(73, 145);
        panel_theme->mButtonGradientTopFocused = nanogui::Color(50, 200);
        panel_theme->mButtonGradientBotFocused = nanogui::Color(50, 200);
        panel_theme->mWindowHeaderGradientTop = nanogui::Color(45, 140);
        panel_theme->mWindowHeaderGradientBot = nanogui::Color(45, 140);
        panel->setTheme(panel_theme);

        // Setup buttons on the panel.
        {
            //Setup Start button. It's a toggle button. Rendering goes on while it's pressed and stops when unpressed.
            nanogui::Button* start_btn = new nanogui::Button(panel, "Start");
            start_btn->setTooltip("Start the Rendering process..");
            start_btn->setCallback([&](){ this->options.start = true; });

            //Setup Stop button. Stop rendering process or close the window altogether, depends on the programmer.
            nanogui::Button* stop_btn = new nanogui::Button(panel, "Stop");
            stop_btn->setTooltip("Stop the Rendering process..");
            stop_btn->setCallback([&](){ this->options.stop = true; });

            //Setup Save button. Allow users to save the currently drawn image.
            nanogui::Button* save_btn = new nanogui::Button(panel, "Save", ENTYPO_ICON_EXPORT);
            save_btn->setTooltip("Quick Save an image..");
            save_btn->setCallback([&](){this->options.save_img = true; });

            // Info button for controls.
            nanogui::Button* info_btn = new nanogui::Button(panel, "Info");
            info_btn->setTooltip("Information on controls..");
            info_btn->setCallback( [&]()
                                  { auto md = new nanogui::MessageDialog(this->screen,
                                                               nanogui::MessageDialog::Type::Information,
                                                               "Controls Information",
                                                               "Hold Space and move mouse to rotate camera. Press \"Stop\" to close window and \"Save\" to save the current frame.");
                                    md->setTheme(this->panel_theme);
                                  });

            //Setup Options popup button. Expose simple options such as sample size and GI check.
            nanogui::PopupButton* popup_btn = new nanogui::PopupButton(panel, "Options");
            nanogui::Popup* popup = popup_btn->popup();

            nanogui::GridLayout* popup_layout = new nanogui::GridLayout(
                                                          nanogui::Orientation::Horizontal, 2,
                                                          nanogui::Alignment::Middle, 15, 5);
            popup_layout->setColAlignment( { nanogui::Alignment::Maximum, nanogui::Alignment::Fill } );
            popup_layout->setSpacing(0, 10);
            popup->setLayout(popup_layout);
            popup->setTheme(panel_theme);

            // Setup widgets inside Popup panel
            {
                // Setup a GI check box.
                {
                    new nanogui::Label(popup, "Global Illumination :", "sans-bold");
                    nanogui::CheckBox* check_box = new nanogui::CheckBox(popup, "",
                                                                         [&](bool check){if(check)
                                                                                            this->options.gi_check = 1;
                                                                                        else
                                                                                            this->options.gi_check = 0;
                                                                                        });
                    check_box->setChecked(true);
                }

                // Int box for Samples at which to save an image.
                {
                    new nanogui::Label(popup, "Save Image at Samples :", "sans-bold");
                    nanogui::IntBox<int>* int_box = new nanogui::IntBox<int>(popup, 0);
                    int_box->setEditable(true);
                    int_box->setSpinnable(true);
                    int_box->setFontSize(16);
                    int_box->setFixedSize(Eigen::Vector2i(100,20));
                    int_box->setMinMaxValues(0, std::numeric_limits<int>::max());
                    int_box->setCallback([&](int x){ this->options.save_img_samples = x; });
                }

                // Text box for Save Image name.
                {
                    new nanogui::Label(popup, "Save Image Filename :", "sans-bold");
                    save_img_name = new nanogui::TextBox(popup, "-");
                    save_img_name->setEditable(true);
                    save_img_name->setFontSize(16);
                    save_img_name->setFixedSize(Eigen::Vector2i(100,20));
                }

                // Text box for file extension of saved image. Valid extensions are {png, jpeg, jpg, jpe, bmp}
                {
                    new nanogui::Label(popup, "Save Extension :", "sans-bold");
                    save_ext = new nanogui::TextBox(popup, ".png");
                    save_ext->setEditable(true);
                    save_ext->setFontSize(16);
                    save_ext->setFixedSize(Eigen::Vector2i(100,20));
                    save_ext->setFormat(".[A-Za-z]*");
                }
            }
        }

        //Setup Profiler window.
        profiler_window = new nanogui::Window(screen, "Profiling");
        nanogui::GridLayout* profiler_layout = new nanogui::GridLayout(
                                                       nanogui::Orientation::Horizontal, 2,
                                                       nanogui::Alignment::Middle, 10, 5);

        profiler_window->setLayout(profiler_layout);
        profiler_window->setTheme(panel_theme);


        //Setup text box to show FPS.
        nanogui::Label* label = new nanogui::Label(profiler_window, "FPS :", "sans-bold");
        label->setFontSize(18);
        fps = new nanogui::IntBox<int>(profiler_window, 0);
        fps->setEditable(false);
        fps->setValue(0);
        fps->setFontSize(16);
        fps->setFixedSize(Eigen::Vector2i(100,20));

        //Setup text box to show ms per frame.
        label = new nanogui::Label(profiler_window, "ms/Frame :", "sans-bold");
        label->setFontSize(18);
        mspf_avg = new nanogui::FloatBox<float>(profiler_window, 0.0f);
        mspf_avg->setUnits("ms");
        mspf_avg->setValue(0.0);
        mspf_avg->setEditable(false);
        mspf_avg->setFontSize(16);
        mspf_avg->setFixedSize(Eigen::Vector2i(100,20));
        mspf_avg->setTooltip("Time taken by 1 frame in ms. Averaged over 1s interval. Slightly more than ms/kernel");

        //Setup text box to show actual kernel execution time averaged over 1 second.
        label = new nanogui::Label(profiler_window, "ms/Kernel :", "sans-bold");
        label->setFontSize(18);
        mspk_avg = new nanogui::FloatBox<float>(profiler_window, 0.0f);
        mspk_avg->setUnits("ms");
        mspk_avg->setValue(0.0);
        mspk_avg->setEditable(false);
        mspk_avg->setFontSize(16);
        mspk_avg->setFixedSize(Eigen::Vector2i(100,20));
        mspk_avg->setTooltip("Kernel execution time in ms. It's averaged over 1 second interval.");

        //Setup text box to show render time.
        label = new nanogui::Label(profiler_window, "Render Time :", "sans-bold");
        label->setFontSize(18);
        render_time = new nanogui::IntBox<int>(profiler_window, 0.0f);
        render_time->setUnits("sec");
        render_time->setValue(0.0);
        render_time->setEditable(false);
        render_time->setFontSize(16);
        render_time->setFixedSize(Eigen::Vector2i(100,20));
        render_time->setTooltip("Total Time passed since the start of rendering.");

        //Setup text box to show total samples per pixel.
        label = new nanogui::Label(profiler_window, "SPP :", "sans-bold");
        label->setFontSize(18);
        tot_samples = new nanogui::IntBox<int>(profiler_window, 0);
        tot_samples->setUnits("spp");
        tot_samples->setEditable(false);
        tot_samples->setValue(0);
        tot_samples->setFontSize(16);
        tot_samples->setFixedSize(Eigen::Vector2i(100,20));
        tot_samples->setTooltip("Total Samples per pixel..");

        screen->performLayout();
        profiler_window->setPosition(Eigen::Vector2i(framebuffer_width - profiler_window->width(), 0));

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
        if(is_fullscreen)
            glfwRestoreWindow(window);
        else
            screen->setVisible(true);

        glfwWindowHint(GLFW_AUTO_ICONIFY, true);
        glfwFocusWindow(window);


    }

    void GL_context::errorCallback(int error, const char* msg)
    {
        std::cout << msg << std::endl;
    }

    void GL_context::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
    {
        GL_context* ptr = (GL_context*) glfwGetWindowUserPointer(window);

        if(key == GLFW_KEY_W &&  (action == GLFW_PRESS || action == GLFW_REPEAT) )
          GL_context::cameraUpdateCallback(Vec4f(0,0,1,0), 0, 0);

        if(key == GLFW_KEY_S &&  (action == GLFW_PRESS || action == GLFW_REPEAT) )
          cameraUpdateCallback(Vec4f(0,0, -1,0), 0, 0);

        if(key == GLFW_KEY_A &&  (action == GLFW_PRESS || action == GLFW_REPEAT) )
           cameraUpdateCallback(Vec4f(-1,0,0,0), 0, 0);

        if(key == GLFW_KEY_D &&  (action == GLFW_PRESS || action == GLFW_REPEAT) )
            cameraUpdateCallback(Vec4f(1,0,0,0), 0, 0);

        if(key == GLFW_KEY_SPACE &&  action == GLFW_PRESS )
            ptr->space_flag = true;
        if(key == GLFW_KEY_SPACE &&  action == GLFW_RELEASE )
            ptr->space_flag = false;

        screen->keyCallbackEvent(key, scancode, action, mods);
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
        screen->cursorPosCallbackEvent(new_cursor_x, new_cursor_y);

    }

    void GL_context::mouseButtonCallback(GLFWwindow* window, int button, int action, int modifiers)
    {
        screen->mouseButtonCallbackEvent(button, action, modifiers);
    }

    void GL_context::charCallback(GLFWwindow *w, unsigned int codepoint)
    {
        screen->charCallbackEvent(codepoint);
    }

    void GL_context::dropCallback(GLFWwindow *w, int count, const char **filenames)
    {
        screen->dropCallbackEvent(count, filenames);
    }

    void GL_context::scrollCallback(GLFWwindow *w, double x, double y)
    {
        screen->scrollCallbackEvent(x, y);
    }

    void GL_context::framebufferSizeCallback(GLFWwindow* window, int width, int height)
    {
        screen->resizeCallbackEvent(width, height);
    }

    void GL_context::setCameraUpdateCallback(std::function<void(const Vec4f&, float, float)> cb)
    {
        GL_context::cameraUpdateCallback = cb;
    }

}
