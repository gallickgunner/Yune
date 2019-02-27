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
        delete[] rk_lws;
        delete[] ppk_lws;
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

        //Next option specifies if compiler options are provided or not, followed by kernel file names and function names.
        if(options[i] != "-")
            cl_manager.createProgram(options[i], options[i+1], options[i+2], options[i+3], options[i+4]);
        else
            cl_manager.createProgram("", options[i+1], options[i+2], options[i+3], options[i+4]);

        i += 5;

        //Next options tells if GWS for Rendering Kernel is provided.
        if(options[i] != "-")
        {
            rk_gws[0] = std::stoi(options[i++]);
            rk_gws[1] = std::stoi(options[i++]);
        }
        else
        {
            rk_gws[0] = glfw_manager.framebuffer_width;
            rk_gws[1] = glfw_manager.framebuffer_height;
            i+=2;;
        }

        //Next option tells if LWS for Rendering Kernel is provided
        if(options[i] != "-")
        {
            rk_lws = new size_t[2];
            rk_lws[0] = std::stoi(options[i++]);
            rk_lws[1] = std::stoi(options[i++]);
        }
        else
        {
            rk_lws = NULL;
            i+=2;
        }

        //Next options tells if GWS for Post-processing Kernel is provided.
        if(options[i] != "-")
        {
            ppk_gws[0] = std::stoi(options[i++]);
            ppk_gws[1] = std::stoi(options[i++]);
        }
        else
        {
            ppk_gws[0] = glfw_manager.framebuffer_width;
            ppk_gws[1] = glfw_manager.framebuffer_height;
            i+=2;;
        }

        //Next options tells if LWS for Post-processing Kernel is provided.
        if(options[i] != "-")
        {
            ppk_lws = new size_t[2];
            ppk_lws[0] = std::stoi(options[i++]);
            ppk_lws[1] = std::stoi(options[i++]);
        }
        else
        {
            ppk_lws = NULL;
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

        /* Pass Scene Model Data. If not present NULL Buffer will be passed. Since other arguments need to be passed
         * regularly, we pass them in the loop inside start function. Scene and material data remain constant hence passed
         * only once here.
         */
        if(!scene_data.empty())
        {
            cl_int err = 0;
            err = clSetKernelArg(cl_manager.rend_kernel, 3, sizeof(cl_mem), &cl_manager.scene_buffer);
            CL_context::checkError(err, __FILE__, __LINE__ -1);

            err = clSetKernelArg(cl_manager.rend_kernel, 4, sizeof(cl_mem), &cl_manager.mat_buffer);
            CL_context::checkError(err, __FILE__, __LINE__ -1);
        }
        else
        {
            cl_int err = 0;
            err = clSetKernelArg(cl_manager.rend_kernel, 3, sizeof(cl_mem), NULL);
            CL_context::checkError(err, __FILE__, __LINE__ -1);

            err = clSetKernelArg(cl_manager.rend_kernel, 4, sizeof(cl_mem), NULL);
            CL_context::checkError(err, __FILE__, __LINE__ -1);
        }


    }

    void Renderer::start()
    {
        int itr = 0, frame_count = 0;
        unsigned long samples_taken = 0;
        bool buffer_switch = true;
        double last_time, current_time, start_time = 0, prev_time;
        double submit_time;             // Time when kernel was submitted to command queue.
        double exec_time_rk = 0.0;      // Rendering Kernel execution time.
        double exec_time_ppk = 0.0;     // Post-processing Kernel execution time.
        float skip_ticks = 16.666;      // in ms. If kernel takes too long to finish. We need to update GUI ever 16.66 ms.
        cl_uint seed;
        cl_int err = 0, reset = 0, kernel_status = CL_COMPLETE;
        cl_event rk_event;

        //Store the value of GI check. Pass as argument only when there is a change
        bool gi_check = 0;

        //Initialize a RNG
        unsigned int mt_seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        std::mt19937 mt_engine(mt_seed);
        std::uniform_int_distribution<unsigned int> dist(0,  std::numeric_limits<unsigned int>::max() );
        glfw_manager.showWindow();

        //Setup RBO as the source from where to read pixel data. Set default framebuffer for writing.
        glBindFramebuffer(GL_READ_FRAMEBUFFER, glfw_manager.fbo_ID);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glDrawBuffer(GL_BACK);
        glfwSetTime(0.f);

        glClearColor(0.7f, 0.7f, 0.7f, 1.0f);
        while(!glfw_manager.gui->stop_btn && !glfwWindowShouldClose(glfw_manager.window))
        {
            // If start button pressed, start rendering
            if(glfw_manager.gui->start_btn)
            {
                if(start_time == 0 || reset == 1)
                    last_time = start_time = glfwGetTime();

                //Update Render Time
                if(glfwGetTime() - prev_time >= 1.0f)
                {
                    prev_time = glfwGetTime();
                    glfw_manager.gui->profiler.render_time->setValue(glfwGetTime() - start_time);
                }

                //If save button was pressed, save the current image lying in the default framebuffer newest image output by kernel.
                if(glfw_manager.gui->save_img_btn)
                {
                    saveImage(samples_taken, (glfwGetTime() - start_time), glfw_manager.gui->profiler.fps->value(), glfw_manager.gui->hdr_check->checked(), buffer_switch);
                    glfw_manager.gui->save_img_btn = false;
                }

                //Enqueue Kernel only if previous kernel completed it's execution
                if(kernel_status == CL_COMPLETE)
                {
                    seed = dist(mt_engine);
                    updateRenderKernelArgs(gi_check, reset, seed, buffer_switch);

                    //Enqueue Kernel.
                    if(!cl_manager.target_device.clgl_event_ext)
                        glFinish();

                    err = clEnqueueAcquireGLObjects(cl_manager.comm_queue, 2, cl_manager.image_buffers, 0, NULL, NULL);
                    CL_context::checkError(err, __FILE__, __LINE__ -1);

                    err = clEnqueueNDRangeKernel(cl_manager.comm_queue,     // command queue
                                                 cl_manager.rend_kernel,    // kernel
                                                 2,                         // global work dimensions
                                                 NULL,                      // global work offset
                                                 rk_gws,                    // global workgroup size
                                                 rk_lws,                    // local workgroup size
                                                 0,                         // Number of events in wait list.
                                                 NULL,                      // Events in wait list
                                                 &rk_event              // Event
                                                );
                    CL_context::checkError(err, __FILE__, __LINE__ -1);

                    //Issue this kernel to the device. Now after this we can start querying whether the kernel finished or not.
                    clFlush(cl_manager.comm_queue);
                    submit_time = glfwGetTime() * 1000.0;

                }

                // Check if kernel completed execution. If so, then apply post-processing kernel (Mainly tone mapping and Gamma correction)
                clGetEventInfo(rk_event, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof(cl_int), &kernel_status, NULL);
                if(kernel_status == CL_COMPLETE)
                {
                    // Get Profiling info for rendering kernel
                    cl_ulong time_start, time_finish;
                    clGetEventProfilingInfo(rk_event, CL_PROFILING_COMMAND_START, sizeof(time_start), &time_start, NULL);
                    clGetEventProfilingInfo(rk_event, CL_PROFILING_COMMAND_END, sizeof(time_finish), &time_finish, NULL);
                    clReleaseEvent(rk_event);

                    exec_time_rk += (time_finish - time_start)/1000000.0;

                    //Enqueue Post-processing kernel
                    cl_event ppk_event;
                    updatePostProcessingKernelArgs(reset, buffer_switch);
                    err = clEnqueueNDRangeKernel(cl_manager.comm_queue,     // command queue
                                                 cl_manager.pp_kernel,      // kernel
                                                 2,                         // global work dimensions
                                                 NULL,                      // global work offset
                                                 ppk_gws,                   // global workgroup size
                                                 ppk_lws,                   // local workgroup size
                                                 0,                         // Number of events in wait list.
                                                 NULL,                      // Events in wait list
                                                 &ppk_event                 // Event
                                                );
                    CL_context::checkError(err, __FILE__, __LINE__ -1);


                    err = clEnqueueReleaseGLObjects(cl_manager.comm_queue, 2, cl_manager.image_buffers, 0, NULL, NULL);
                    CL_context::checkError(err, __FILE__, __LINE__ -1);

                    clFinish(cl_manager.comm_queue);

                    // Get Profiling info for post-processing kernel
                    clGetEventProfilingInfo(ppk_event, CL_PROFILING_COMMAND_START, sizeof(time_start), &time_start, NULL);
                    clGetEventProfilingInfo(ppk_event, CL_PROFILING_COMMAND_END, sizeof(time_finish), &time_finish, NULL);
                    clReleaseEvent(ppk_event);
                    exec_time_ppk += (time_finish - time_start)/1000000.0;

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

                    //Display total samples taken uptill now.
                    if(reset == 1)
                        samples_taken = 1;
                    else
                        samples_taken++;

                    glfw_manager.gui->profiler.tot_samples->setValue(samples_taken);

                    //  If samples taken is equal to the option specified at which to take a screen shot, save the image.
                    if(samples_taken > 0 && samples_taken == glfw_manager.gui->save_img_samples->value())
                        saveImage(samples_taken, (glfwGetTime()- start_time), glfw_manager.gui->profiler.fps->value(), glfw_manager.gui->hdr_check->checked(), buffer_switch);

                    // Measure FPS averaged over 1 second interval.
                    frame_count++;
                    current_time = glfwGetTime();
                    if(current_time - last_time >= 1.0)
                    {
                        glfw_manager.gui->profiler.mspf_avg->setValue( (current_time-last_time)*1000.0/frame_count  );
                        glfw_manager.gui->profiler.msrk_avg->setValue(exec_time_rk/frame_count);
                        glfw_manager.gui->profiler.msppk_avg->setValue(exec_time_ppk/frame_count);
                        glfw_manager.gui->profiler.fps->setValue(frame_count);

                        std::string title = "Yune, ms/frame: ";
                        title += glfw_manager.gui->profiler.mspf_avg->value();
                        glfwSetWindowTitle(glfw_manager.window, title.data());

                        frame_count = 0;
                        last_time = current_time;
                        exec_time_rk = exec_time_ppk = 0.0;
                    }
                    itr++;
                }
            }

            // Check the time passed since kernel was enqueued. If time passed is greater than skip_ticks or if the kernel finished execution draw GUI and swapbuffers.
            current_time = glfwGetTime() * 1000.0;
            if( kernel_status == CL_COMPLETE || ( (current_time - submit_time) >= skip_ticks)  )
            {
                submit_time = current_time;

                glClear(GL_COLOR_BUFFER_BIT);
                if(glfw_manager.gui->start_btn)
                {
                    glReadBuffer(GL_COLOR_ATTACHMENT2);
                    glBlitFramebuffer(0, 0, glfw_manager.framebuffer_width, glfw_manager.framebuffer_height,
                                      0, 0, glfw_manager.framebuffer_width, glfw_manager.framebuffer_height,
                                      GL_COLOR_BUFFER_BIT,
                                      GL_LINEAR
                                     );
                }

                glfw_manager.gui->screen->drawWidgets();
                glfwSwapBuffers(glfw_manager.window);
                glfwPollEvents();
            }
        }

    }
    void Renderer::updateRenderKernelArgs(bool& gi_check, cl_int& reset, cl_uint seed, bool buffer_switch)
    {
        cl_int err = 0;
        cl_int new_reset = 0;

        /* Switch Buffers. The image written to earlier is now read only. The color value is read from it the averaged with the new color
         * computed and written to the image that was previously read-only.
         */
        if(buffer_switch)
        {
            err = clSetKernelArg(cl_manager.rend_kernel, 0, sizeof(cl_manager.image_buffers[0]), &cl_manager.image_buffers[0]);
            CL_context::checkError(err, __FILE__, __LINE__ -1);

            err = clSetKernelArg(cl_manager.rend_kernel, 1, sizeof(cl_manager.image_buffers[1]), &cl_manager.image_buffers[1]);
            CL_context::checkError(err, __FILE__, __LINE__ -1);
        }
        else
        {
            err = clSetKernelArg(cl_manager.rend_kernel, 0, sizeof(cl_manager.image_buffers[1]), &cl_manager.image_buffers[1]);
            CL_context::checkError(err, __FILE__, __LINE__ -1);

            err = clSetKernelArg(cl_manager.rend_kernel, 1, sizeof(cl_manager.image_buffers[0]), &cl_manager.image_buffers[0]);
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
                                                                      sizeof(Cam),
                                                                      0,
                                                                      NULL,
                                                                      NULL,
                                                                      &err
                                                                     );
            CL_context::checkError(err, __FILE__, __LINE__ -1);

            render_scene->main_camera.setBuffer(mapped_memory);

            err = clEnqueueUnmapMemObject(cl_manager.comm_queue, cl_manager.camera_buffer, mapped_memory, 0, NULL, NULL);
            CL_context::checkError(err, __FILE__, __LINE__ -1);

            err = clSetKernelArg(cl_manager.rend_kernel, 2, sizeof(cl_mem), &cl_manager.camera_buffer);
            CL_context::checkError(err, __FILE__, __LINE__ -1);


        }

        // Pass GI check value. Change the reset value to 1.
        if(gi_check != glfw_manager.gui->gi_check->checked())
        {
            gi_check = glfw_manager.gui->gi_check->checked();
            cl_int check = gi_check;

            err = clSetKernelArg(cl_manager.rend_kernel, 5, sizeof(cl_int), &check);
            CL_context::checkError(err, __FILE__, __LINE__ -1);
            new_reset = 1;
        }

        // Pass reset value. If there was no change in reset value, no need to pass again.
        if (reset != new_reset)
        {
            reset = new_reset;
            err = clSetKernelArg(cl_manager.rend_kernel, 6, sizeof(cl_int), &reset);
            CL_context::checkError(err, __FILE__, __LINE__ -1);
        }

        // Pass a random seed value.
        err = clSetKernelArg(cl_manager.rend_kernel, 7, sizeof(cl_uint), &seed);
        CL_context::checkError(err, __FILE__, __LINE__ -1);

    }

    void Renderer::updatePostProcessingKernelArgs(cl_int reset, bool buffer_switch)
    {
        cl_int err = 0;

        //Pass Current frame
        if(buffer_switch)
            err = clSetKernelArg(cl_manager.pp_kernel, 0, sizeof(cl_manager.image_buffers[0]), &cl_manager.image_buffers[0]);
        else
            err = clSetKernelArg(cl_manager.pp_kernel, 0, sizeof(cl_manager.image_buffers[1]), &cl_manager.image_buffers[1]);
        CL_context::checkError(err, __FILE__, __LINE__ -1);

        //Pass Previous frame
        if(buffer_switch)
            err = clSetKernelArg(cl_manager.pp_kernel, 1, sizeof(cl_manager.image_buffers[1]), &cl_manager.image_buffers[1]);
        else
            err = clSetKernelArg(cl_manager.pp_kernel, 1, sizeof(cl_manager.image_buffers[0]), &cl_manager.image_buffers[0]);
        CL_context::checkError(err, __FILE__, __LINE__ -1);

        //Pass Output Image to contain tonemapped data
        err = clSetKernelArg(cl_manager.pp_kernel, 2, sizeof(cl_manager.image_buffers[2]), &cl_manager.image_buffers[2]);
        CL_context::checkError(err, __FILE__, __LINE__ -1);

        //Pass Reset Value. This tells the kernel if previous frame is available for calculating exposure value
        err = clSetKernelArg(cl_manager.pp_kernel, 3, sizeof(cl_int), &reset);
        CL_context::checkError(err, __FILE__, __LINE__ -1);
    }

    void Renderer::saveImage(unsigned long samples_taken, int time_passed, int fps, bool hdr_check, bool buffer_switch)
    {
        int width = glfw_manager.framebuffer_width;
        int height = glfw_manager.framebuffer_height;
        int bpp; // bits per pixel
        int scanline_width = (width % 4) + (width * 3);
        std::string ext, fn;
        FIBITMAP* bitmap;

        if(hdr_check)
        {
            if(buffer_switch)
                glReadBuffer(GL_COLOR_ATTACHMENT0);
            else
                glReadBuffer(GL_COLOR_ATTACHMENT1);
            float* img_data = new float[scanline_width*height*3];
            bpp = 96;
            ext =  ".exr";
            glReadPixels(0, 0, width, height, GL_RGB, GL_FLOAT, img_data);
            bitmap = FreeImage_AllocateT(FIT_RGBF, width, height, 96);
            for(int y = 0, z = 0; y < height; y++)
            {
                z += (width%4);
                FIRGBF* bits = (FIRGBF*) FreeImage_GetScanLine(bitmap, y);
                for(int x = 0; x < width; x++)
                {
                    bits[x].red = img_data[z++];
                    bits[x].green = img_data[z++];
                    bits[x].blue = img_data[z++];
                }
            }
            fn = glfw_manager.gui->save_img_name->value() + "-" + std::to_string(samples_taken) + "spp-" + std::to_string(time_passed) + "sec-" + std::to_string(fps) + "fps" + ext;
            FreeImage_Save(FIF_EXR, bitmap, fn.data(), EXR_FLOAT|EXR_PIZ);
            delete[] img_data;
        }
        else
        {
            unsigned char* img_data = new unsigned char[scanline_width*height*3];
            bpp = 24;
            ext =  ".jpg";
            glReadBuffer(GL_COLOR_ATTACHMENT2);
            glReadPixels(0, 0, width, height, GL_BGR, GL_UNSIGNED_BYTE, img_data);
            bitmap = FreeImage_ConvertFromRawBits(img_data, width, height, scanline_width, bpp, FI_RGBA_RED_MASK, FI_RGBA_GREEN_MASK, FI_RGBA_BLUE_MASK, false);

            fn = glfw_manager.gui->save_img_name->value() + "-" + std::to_string(samples_taken) + "spp-" + std::to_string(time_passed) + "sec-" + std::to_string(fps) + "fps" + ext;
            FreeImage_Save(FIF_JPEG, bitmap, fn.data(), JPEG_QUALITYSUPERB);
            delete[] img_data;

            //Switch buffers back.
            if(buffer_switch)
                glReadBuffer(GL_COLOR_ATTACHMENT0);
            else
                glReadBuffer(GL_COLOR_ATTACHMENT1);
        }

    }
}
