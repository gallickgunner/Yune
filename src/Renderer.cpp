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


#ifdef _WIN32
#include <windows.h>
#endif // defined _WIN32

#include <RuntimeError.h>
#include <Renderer.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <random>
#include <chrono>
#include <limits>

namespace yune
{
    Renderer::Renderer()
    {
        //ctor
    }

    Renderer::~Renderer()
    {
        delete[] lws;
        //dtor
    }

    void Renderer::setup(Scene* render_scene)
    {
        this->render_scene = render_scene;

        /* Read renderer options from a file. This allows us our application to be independent of compile time. Simply stating,
         * we wouldn't have to re-compile if you change image width/height or kernel file. Very useful when debugging and trying
         * multiple kernel variations.
         */

        std::fstream file;
        std::vector<std::string> options;
        file.open("yune-options.yun");
        if(file.is_open())
        {
            std::string line;
            while(getline(file, line))
            {
                // skip comments and empty lines
                if(line[0] == '#' || line.empty())
                    continue;

                options.push_back(line);
            }
            file.close();
        }

        if(options.empty())
            throw RuntimeError("Please create a file named <renderer-options.yun> and provide all the options.");

        int i = 0;
        //Start Processing options.

        //First 3 options are width, height and fullscreen
        glfw_manager.createWindow(std::stoi(options[i]), std::stoi(options[i+1]), std::stoi(options[i+2]));
        cl_manager.setup();
        i += 3;

        //Next option specifies if compiler options are provided or not, followed by kernel file name and function name.
        if(options[i] != "-")
            cl_manager.createProgram(options[i], options[i+1], options[i+2]);
        else
            cl_manager.createProgram("", options[i+1], options[i+2]);

        i += 3;

        //Next options tells if GWS is provided.
        if(options[i] != "-")
        {
            gws[0] = std::stoi(options[i++]);
            gws[1] = std::stoi(options[i++]);
        }
        else
        {
            gws[0] = glfw_manager.framebuffer_width;
            gws[1] = glfw_manager.framebuffer_height;
            i+=2;;
        }

        //Next option tells if LWS is provided
        if(options[i] != "-")
        {
            lws = new size_t[2];
            lws[0] = std::stoi(options[i++]);
            lws[1] = std::stoi(options[i++]);
        }
        else
        {
            lws = NULL;
            i+=2;
        }

        //Next Option tells if there is a model file to load
        if(options[i] != "-")
            render_scene->loadModel(options[i], scene_data,  options[i+1], mat_data);

        /** Since GLFW manager don't have access to camera variable, pass Camera's function as callback.
            This function is called when Camera is updated through mouse/keyboard.
         */
        glfw_manager.setCameraUpdateCallback(std::bind( &(render_scene->main_camera.setOrientation),
                                                    &(render_scene->main_camera),
                                                    std::placeholders::_1,
                                                    std::placeholders::_2,
                                                    std::placeholders::_3)
                                         );

        /* Setup Buffer data to pass to CL_context. Set is_changed to true initially so path tracer
           only writes current color instead of merging it with previous.
         */
        this->render_scene->main_camera.setBuffer(&cam_data);
        this->render_scene->main_camera.is_changed = true;

        glfw_manager.setupGlBuffer();
        cl_manager.setupBuffers(glfw_manager.rbo_IDs, scene_data, mat_data, &cam_data);

        //Pass Scene Model Data if present.
        if(!scene_data.empty())
        {
            cl_int err = 0;
            err = clSetKernelArg(cl_manager.kernel, 6, sizeof(cl_mem), &cl_manager.scene_buffer);
            CL_context::checkError(err, __FILE__, __LINE__ -1);

            err = clSetKernelArg(cl_manager.kernel, 7, sizeof(cl_mem), &cl_manager.bsdf_buffer);
            CL_context::checkError(err, __FILE__, __LINE__ -1);
        }
    }

    void Renderer::start()
    {
        int itr = 0, frame_count = 0;
        unsigned long samples_taken = 0;
        bool buffer_switch = true;
        double last_time, current_time, start_time = 0;
        double submit_time;             // Time when kernel was submitted to command queue.
        double exec_time = 0.0;         // Kernel execution time.
        double exec_time_avg = 0.0;     // Kernel execution time averaged.
        double skip_ticks = 33.3333;    // in milliseconds. We need to update GUI after every 33.33 ms

        cl_uint seed;
        cl_int err = 0, reset = 0, kernel_status = CL_COMPLETE;
        cl_event kernel_event;

        /** Declare a temporary Options structure to check **when** there is a change in options.
            Pass to kernel as arguments only when there is a change
         */
        GL_context::Options options;
        options.gi_check = -1;
        options.save_img_samples = 0;

        unsigned int mt_seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        std::mt19937 mt_engine(mt_seed);
        std::uniform_int_distribution<unsigned int> dist(0,  std::numeric_limits<unsigned int>::max() );
        glfw_manager.showWindow();

        //Set RBO as the source from where to read pixel data. Since Read Framebuffer never changes no need to bind it inside loop.
        glBindFramebuffer(GL_READ_FRAMEBUFFER, glfw_manager.fbo_ID);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glfwSetTime(0.f);

        last_time = glfwGetTime();
        glClearColor(0.7f, 0.7f, 0.7f, 1.0f);
        while(!glfw_manager.options.stop && !glfwWindowShouldClose(glfw_manager.window))
        {
            // If start button pressed, start rendering
            if(glfw_manager.options.start)
            {
                if(start_time == 0 || reset == 1)
                    start_time = glfwGetTime();

                //If save button was pressed, save the current image lying in the default framebuffer newest image output by kernel.
                if(glfw_manager.options.save_img)
                {
                    saveImage(samples_taken, (glfwGetTime() - start_time), glfw_manager.fps->value());
                    glfw_manager.options.save_img = false;
                }

                //Enqueue Kernel only if previous kernel completed it's execution
                if(kernel_status == CL_COMPLETE)
                {
                    //Set Render Time.
                    glfw_manager.render_time->setValue(glfwGetTime() - start_time);

                    seed = dist(mt_engine);
                    updateKernelArguments(options, reset, seed, buffer_switch);

                    //Enqueue Kernel.
                    if(!cl_manager.target_device.ext_supported)
                        glFinish();

                    err = clEnqueueAcquireGLObjects(cl_manager.comm_queue, 2, cl_manager.image_buffers, 0, NULL, NULL);
                    CL_context::checkError(err, __FILE__, __LINE__ -1);

                    err = clEnqueueNDRangeKernel(cl_manager.comm_queue,     // command queue
                                                 cl_manager.kernel,         // kernel
                                                 2,                         // global work dimensions
                                                 NULL,                      // global work offset
                                                 gws,                       // global workgroup size
                                                 lws,                       // local workgroup size
                                                 0,                         // Number of events in wait list.
                                                 NULL,                      // Events in wait list
                                                 &kernel_event              // Event
                                                );
                    CL_context::checkError(err, __FILE__, __LINE__ -1);

                    //Issue this kernel to the device. Now after this we can start querying whether the kernel finished or not.
                    clFlush(cl_manager.comm_queue);
                    submit_time = glfwGetTime() * 1000.0;

                }

                // Check if kernel completed execution, If so, blit the RBO to the default framebuffer's back buffer.
                clGetEventInfo(kernel_event, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof(cl_int), &kernel_status, NULL);
                if(kernel_status == CL_COMPLETE)
                {
                    // Get Profiling info
                    cl_ulong time_start, time_finish;
                    clGetEventProfilingInfo(kernel_event, CL_PROFILING_COMMAND_START, sizeof(time_start), &time_start, NULL);
                    clGetEventProfilingInfo(kernel_event, CL_PROFILING_COMMAND_END, sizeof(time_finish), &time_finish, NULL);
                    clReleaseEvent(kernel_event);

                    exec_time = (time_finish - time_start)/1000000.0;
                    exec_time_avg += exec_time;

                    err = clEnqueueReleaseGLObjects(cl_manager.comm_queue, 2, cl_manager.image_buffers, 0, NULL, NULL);
                    CL_context::checkError(err, __FILE__, __LINE__ -1);

                    if(!cl_manager.target_device.ext_supported)
                        clFinish(cl_manager.comm_queue);

                    if(buffer_switch)
                    {
                        glReadBuffer(GL_COLOR_ATTACHMENT0);
                        buffer_switch = false;
                    }
                    else
                    {
                        glReadBuffer(GL_COLOR_ATTACHMENT1);
                        buffer_switch = true;
                    }

                    // Save a copy. This is needed if kernel takes longer than 33.33 ms to finish.
                    if( (int)exec_time > (int)skip_ticks)
                    {
                        //Set default framebuffer as destination
                        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, glfw_manager.fbo_ID);
                        glDrawBuffer(GL_COLOR_ATTACHMENT2);
                        glClear(GL_COLOR_BUFFER_BIT);
                        glBlitFramebuffer(0, 0, glfw_manager.framebuffer_width, glfw_manager.framebuffer_height,
                                          0, 0, glfw_manager.framebuffer_width, glfw_manager.framebuffer_height,
                                          GL_COLOR_BUFFER_BIT,
                                          GL_LINEAR
                                          );
                    }

                    //Set default draw framebuffer as destination
                    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
                    glDrawBuffer(GL_BACK);
                    glClear(GL_COLOR_BUFFER_BIT);
                    glBlitFramebuffer(0, 0, glfw_manager.framebuffer_width, glfw_manager.framebuffer_height,
                                      0, 0, glfw_manager.framebuffer_width, glfw_manager.framebuffer_height,
                                      GL_COLOR_BUFFER_BIT,
                                      GL_LINEAR
                                      );

                    //Display total samples taken uptill now.
                    if(reset == 1)
                        samples_taken = 1;
                    else
                        samples_taken++;

                    glfw_manager.tot_samples->setValue(samples_taken);

                    /*  If samples taken is equal to the option specified at which to take a screen shot, save the image.
                     *  Check if the current value of options is equal to glfw_manager.options. If it's equal this means we have already
                     *  taken a screenshot.
                     */
                    if(samples_taken > 0 && samples_taken == glfw_manager.options.save_img_samples)
                    {
                        saveImage(samples_taken, (glfwGetTime()- start_time), glfw_manager.fps->value());
                        options.save_img_samples = glfw_manager.options.save_img_samples;
                    }

                    // Measure FPS averaged over 1 second interval.
                    frame_count++;
                    current_time = glfwGetTime();
                    if(current_time - last_time >= 1.0)
                    {
                        std::string title = "Yune, ms/frame: ";
                        title += std::to_string(exec_time_avg/frame_count);
                        glfw_manager.mspf_avg->setValue( (current_time-last_time)*1000.0/frame_count  );
                        glfw_manager.mspk_avg->setValue(exec_time_avg/frame_count);
                        glfw_manager.fps->setValue(frame_count);
                        glfwSetWindowTitle(glfw_manager.window, title.data());

                        frame_count = 0;
                        last_time = current_time;
                        exec_time_avg = 0.0;
                    }
                    itr++;
                }
            }

            // Check the time passed since kernel was enqueued. If time passed is greater than 33.33ms or if the kernel finished execution draw GUI and swapbuffers.
            current_time = glfwGetTime() * 1000.0;
            if( kernel_status == CL_COMPLETE || ( (current_time - submit_time) >= skip_ticks)  )
            {
                submit_time = current_time;

                /** If rendering hasn't started yet or if the kernel didn't finish, clear the framebuffer.
                 *  If kernel finished the framebuffer is cleared above when blitting to the default framebuffer.
                 */
                if(kernel_status != CL_COMPLETE || !glfw_manager.options.start)
                    glClear(GL_COLOR_BUFFER_BIT);

                if(kernel_status != CL_COMPLETE && exec_time != 0.0)
                {
                    glReadBuffer(GL_COLOR_ATTACHMENT2);
                    glBlitFramebuffer(0, 0, glfw_manager.framebuffer_width, glfw_manager.framebuffer_height,
                                      0, 0, glfw_manager.framebuffer_width, glfw_manager.framebuffer_height,
                                      GL_COLOR_BUFFER_BIT,
                                      GL_LINEAR
                                     );
                }

                glfw_manager.screen->drawWidgets();
                glfwSwapBuffers(glfw_manager.window);
                glfwPollEvents();

            }

        }

    }
    void Renderer::updateKernelArguments(GL_context::Options& opts, cl_int& reset, cl_uint seed, bool buffer_switch)
    {
        cl_int err = 0;
        cl_int new_reset = 0;

        /* Switch Buffers. The image written to earlier is now read only. The color value is read from it the averaged with the new color
         * computed and written to the image that was previously read-only.
         */
        if(buffer_switch)
        {
            err = clSetKernelArg(cl_manager.kernel, 0, sizeof(cl_manager.image_buffers[0]), &cl_manager.image_buffers[0]);
            CL_context::checkError(err, __FILE__, __LINE__ -1);

            err = clSetKernelArg(cl_manager.kernel, 1, sizeof(cl_manager.image_buffers[1]), &cl_manager.image_buffers[1]);
            CL_context::checkError(err, __FILE__, __LINE__ -1);
        }
        else
        {
            err = clSetKernelArg(cl_manager.kernel, 0, sizeof(cl_manager.image_buffers[1]), &cl_manager.image_buffers[1]);
            CL_context::checkError(err, __FILE__, __LINE__ -1);

            err = clSetKernelArg(cl_manager.kernel, 1, sizeof(cl_manager.image_buffers[0]), &cl_manager.image_buffers[0]);
            CL_context::checkError(err, __FILE__, __LINE__ -1);
        }

        // Set the reset flag to 1 when Camera changes orientation. This configures kernel to write the current color instead of averaging it with the previous one.
        if(render_scene->main_camera.is_changed)
        {
            new_reset = 1;
            Cam* mapped_memory = (Cam*) clEnqueueMapBuffer( cl_manager.comm_queue,
                                                                      cl_manager.camera_buffer,
                                                                      CL_TRUE,
                                                                      CL_MAP_WRITE,
                                                                      (size_t) 0,
                                                                      sizeof(cl_manager.camera_buffer),
                                                                      0,
                                                                      NULL,
                                                                      NULL,
                                                                      &err
                                                                     );
            CL_context::checkError(err, __FILE__, __LINE__ -1);

            render_scene->main_camera.setBuffer(mapped_memory);

            err = clEnqueueUnmapMemObject(cl_manager.comm_queue, cl_manager.camera_buffer, mapped_memory, 0, NULL, NULL);
            CL_context::checkError(err, __FILE__, __LINE__ -1);

            err = clSetKernelArg(cl_manager.kernel, 2, sizeof(cl_mem), &cl_manager.camera_buffer);
            CL_context::checkError(err, __FILE__, __LINE__ -1);


        }

        // Pass GI check value. Change the reset value to 1.
        if(opts.gi_check != glfw_manager.options.gi_check)
        {
            opts.gi_check = glfw_manager.options.gi_check;
            cl_int check = opts.gi_check;

            err = clSetKernelArg(cl_manager.kernel, 3, sizeof(cl_int), &check);
            CL_context::checkError(err, __FILE__, __LINE__ -1);
            new_reset = 1;
        }

        // Pass reset value. If there was no change in reset value, no need to pass again.
        if (reset != new_reset)
        {
            reset = new_reset;
            err = clSetKernelArg(cl_manager.kernel, 4, sizeof(cl_int), &reset);
            CL_context::checkError(err, __FILE__, __LINE__ -1);
        }

        // Pass a random seed value.
        err = clSetKernelArg(cl_manager.kernel, 5, sizeof(cl_uint), &seed);
        CL_context::checkError(err, __FILE__, __LINE__ -1);

    }

    void Renderer::saveImage(unsigned long samples_taken, int time_passed, int fps)
    {
        int width = glfw_manager.framebuffer_width;
        int height = glfw_manager.framebuffer_height;
        int bpp = 24; // bits per pixel
        int scanline_width = (width%4)+(width*3);
        unsigned char* img_data = new unsigned char[scanline_width*height*3];

        std::string ext =  glfw_manager.save_ext->value();
        glReadPixels(0, 0, width, height, GL_BGR, GL_UNSIGNED_BYTE, img_data);

        FIBITMAP* bitmap = FreeImage_ConvertFromRawBits(img_data, width, height, scanline_width, bpp, FI_RGBA_RED_MASK, FI_RGBA_GREEN_MASK, FI_RGBA_BLUE_MASK, false);
        std::string fn = glfw_manager.save_img_name->value() + "-" + std::to_string(samples_taken) + "spp-" + std::to_string(time_passed) + "sec-" + std::to_string(fps) + "fps" + ext;
        FREE_IMAGE_FORMAT extension;

        if(ext == ".png")
            extension = FIF_PNG;
        else if(ext== ".bmp")
            extension = FIF_BMP;
        else if(ext == ".jpeg" || ext == ".jpg" || ext == ".jpe")
            extension = FIF_JPEG;
        else
            return; // Unsupported type.

        FreeImage_Save(extension, bitmap, fn.data(), 0);
        delete[] img_data;
    }
}
