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


#ifdef _WIN32
#include <windows.h>
#endif // defined _WIN32

#include <RuntimeError.h>
#include <Renderer.h>
#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <limits>

namespace helion_render
{

    Renderer::Renderer() : cam_data(17)
    {

        //ctor
    }

    Renderer::~Renderer()
    {
        //dtor
    }

    void Renderer::setup(GL_context* glfw_manager, CL_context* cl_manager, Scene* render_scene, std::string save_filename, std::string ext)
    {
        this->glfw_manager = glfw_manager;
        this->cl_manager = cl_manager;
        this->render_scene = render_scene;
        this->save_filename = save_filename;
        this->ext = ext;

        try
        {
            this->glfw_manager->setupGlBuffer();

            /** Since GLFW manager don't have access to camera variable, pass Camera's function as callback.
                This function is called when Camera is updated through mouse/keyboard.
             */
            this->glfw_manager->setCameraUpdateCallback(std::bind( &(render_scene->main_camera.setOrientation),
                                                        &(render_scene->main_camera),
                                                        std::placeholders::_1,
                                                        std::placeholders::_2,
                                                        std::placeholders::_3)
                                             );

            /* Setup Buffer data to pass to CL_context. Set is_changed to true initially so path tracer
               only writes current color instead of merging it with previous.
             */
            this->render_scene->main_camera.setBuffer(cam_data.data());
            this->render_scene->main_camera.is_changed = true;

            this->cl_manager->setupBuffers(glfw_manager->rbo_IDs, scene_data, cam_data);

        }
        catch(const RuntimeError& err)
        {
            //std::cout << "Renderer failed to setup properly. Please refer to the error messages.\n" << std::endl;
            throw;
        }
    }

    void Renderer::start(size_t global_workgroup_size[2], size_t local_workgroup_size[2])
    {
        int itr = 0;
        bool buffer_switch = true;
        int frame_count = 0;
        unsigned long samples_taken = 0;
        double last_time, current_time;
        double submit_time;             // Time when kernel was submitted to command queue.
        double exec_time = 0.0;         // Kernel execution time.
        double exec_time_avg = 0.0;     // Kernel execution time averaged.
        double skip_ticks = 33.3333;    // in milliseconds. We need to update GUI after every 33.33 ms

        cl_uint seed;
        cl_int err = 0, reset = 1, kernel_status = CL_COMPLETE;
        cl_event kernel_event;

        /** Declare a temporary Options structure to check **when** there is a change in options.
            Pass to kernel as arguments only when there is a change
         */
        GL_context::Options options;
        options.gi_check = -1;
        options.samples_x = -1;
        options.samples_y = -1;
        options.gi_ray_bounces = -1;
        options.save_img_samples = -1;

        std::string name;
        std::random_device rng;
        std::mt19937 mt_engine( rng() );
        std::uniform_int_distribution<unsigned int> dist(0,  std::numeric_limits<unsigned int>::max() );

        glfw_manager->showWindow();

        //Set RBO as the source from where to read pixel data. Since Read Framebuffer never changes no need to bind it inside loop.
        glBindFramebuffer(GL_READ_FRAMEBUFFER, glfw_manager->fbo_ID);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glfwSetTime(0.f);
        last_time = glfwGetTime();
        while(!glfw_manager->options.stop && !glfwWindowShouldClose(glfw_manager->window))
        {
            // If start button pressed, start rendering
            if(glfw_manager->options.start)
            {
                //Enqueue Kernel only if previous kernel completed it's execution
                if(kernel_status == CL_COMPLETE)
                {
                    seed = dist(mt_engine);
                    updateKernelArguments(options, reset, seed, buffer_switch);

                    //Enqueue Kernel.
                    if(!cl_manager->target_device.ext_supported)
                        glFinish();

                    err = clEnqueueAcquireGLObjects(cl_manager->comm_queue, 2, cl_manager->image_buffers, 0, NULL, NULL);
                    CL_context::checkError(err, __FILE__, __LINE__ -1);

                    err = clEnqueueNDRangeKernel(cl_manager->comm_queue,    // command queue
                                                 cl_manager->kernel,        // kernel
                                                 2,                         // global work dimensions
                                                 NULL,                      // global work offset
                                                 global_workgroup_size,     // global workgroup size
                                                 local_workgroup_size,      // local workgroup size
                                                 0,                         // Number of events in wait list.
                                                 NULL,                      // Events in wait list
                                                 &kernel_event              // Event
                                                );
                    CL_context::checkError(err, __FILE__, __LINE__ -1);

                    //Issue this kernel to the device. Now after this we can start querying whether the kernel finished or not.
                    clFlush(cl_manager->comm_queue);
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

                    err = clEnqueueReleaseGLObjects(cl_manager->comm_queue, 2, cl_manager->image_buffers, 0, NULL, NULL);
                    CL_context::checkError(err, __FILE__, __LINE__ -1);

                    if(!cl_manager->target_device.ext_supported)
                        clFinish(cl_manager->comm_queue);

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
                        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, glfw_manager->fbo_ID);
                        glDrawBuffer(GL_COLOR_ATTACHMENT2);
                        glClear(GL_COLOR_BUFFER_BIT);
                        glBlitFramebuffer(0, 0, glfw_manager->framebuffer_width, glfw_manager->framebuffer_height,
                                          0, 0, glfw_manager->framebuffer_width, glfw_manager->framebuffer_height,
                                          GL_COLOR_BUFFER_BIT,
                                          GL_LINEAR
                                          );
                    }

                    //Set default draw framebuffer as destination
                    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
                    glDrawBuffer(GL_BACK);
                    glClear(GL_COLOR_BUFFER_BIT);
                    glBlitFramebuffer(0, 0, glfw_manager->framebuffer_width, glfw_manager->framebuffer_height,
                                      0, 0, glfw_manager->framebuffer_width, glfw_manager->framebuffer_height,
                                      GL_COLOR_BUFFER_BIT,
                                      GL_LINEAR
                                      );

                    //Display total samples taken uptill now.
                    if(reset == 0)
                        samples_taken += options.samples_x * options.samples_y;
                    else
                    {
                        samples_taken = 0;
                        samples_taken += options.samples_x * options.samples_y;
                    }
                    glfw_manager->tot_samples->setValue(samples_taken);

                    //If save button was pressed, save this newest image output by kernel.
                    if(glfw_manager->options.save_img)
                    {
                        saveImage(samples_taken);
                        glfw_manager->options.save_img = false;
                    }

                    /*  If samples taken is equal or greater than the option specified at which to take a screen shot, save the image.
                     *  Check if the current value of options is equal to glfw_manager->options. If it's equal this means we have already
                     *  taken a screenshot.
                     */
                    if(options.save_img_samples != glfw_manager->options.save_img_samples && samples_taken >= glfw_manager->options.save_img_samples)
                    {
                        saveImage(samples_taken);
                        options.save_img_samples = glfw_manager->options.save_img_samples;
                    }

                    // Measure FPS averaged over 1 second interval.
                    frame_count++;
                    current_time = glfwGetTime();
                    //glfw_manager->mspf_avg->setValue( (current_time-last_time)*1000.0 );
                    //last_time = current_time;
                    if(current_time - last_time >= 1.0)
                    {
                        std::string title = "Helion-Render, ms/frame: ";
                        title += std::to_string(exec_time_avg/frame_count);
                        glfw_manager->mspf_avg->setValue( (current_time-last_time)*1000.0/frame_count  );
                        glfw_manager->mspk_avg->setValue(exec_time_avg/frame_count);
                        glfw_manager->fps->setValue(frame_count);
                        glfwSetWindowTitle(glfw_manager->window, title.data());

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
                if(kernel_status != CL_COMPLETE || !glfw_manager->options.start)
                    glClear(GL_COLOR_BUFFER_BIT);

                if(kernel_status != CL_COMPLETE && exec_time != 0.0)
                {
                    glReadBuffer(GL_COLOR_ATTACHMENT2);
                    glBlitFramebuffer(0, 0, glfw_manager->framebuffer_width, glfw_manager->framebuffer_height,
                                      0, 0, glfw_manager->framebuffer_width, glfw_manager->framebuffer_height,
                                      GL_COLOR_BUFFER_BIT,
                                      GL_LINEAR
                                     );
                }

                glfw_manager->screen->drawWidgets();
                glfwSwapBuffers(glfw_manager->window);
                glfwPollEvents();

            }
        }

    }
    void Renderer::updateKernelArguments(GL_context::Options& opts, cl_int& reset, cl_uint seed, bool buffer_switch)
    {
        cl_int err = 0;
        // Pass SAMPLES_X, the samples taken in X dimension.
        if(opts.samples_x != glfw_manager->options.samples_x)
        {
            opts.samples_x = glfw_manager->options.samples_x;
            err = clSetKernelArg(cl_manager->kernel, 3, sizeof(cl_int), &opts.samples_x);
            CL_context::checkError(err, __FILE__, __LINE__ -1);
        }

        // Pass SAMPLES_Y, the samples taken in Y dimension.
        if(opts.samples_y != glfw_manager->options.samples_y)
        {
            opts.samples_y = glfw_manager->options.samples_y;
            err = clSetKernelArg(cl_manager->kernel, 4, sizeof(cl_int), &opts.samples_y);
            CL_context::checkError(err, __FILE__, __LINE__ -1);
        }

        // Pass the number of GI ray bounces allowed.
        if(opts.gi_ray_bounces != glfw_manager->options.gi_ray_bounces)
        {
            opts.gi_ray_bounces = glfw_manager->options.gi_ray_bounces;

            err = clSetKernelArg(cl_manager->kernel, 5, sizeof(cl_int), &opts.gi_ray_bounces);
            CL_context::checkError(err, __FILE__, __LINE__ -1);
        }

        // Set reset flag to 1 when Camera changes orientation or if GI is checked/unchecked. This configures kernel to write the current color instead of averaging it with the previous one.
        if(render_scene->main_camera.is_changed || (opts.gi_check != glfw_manager->options.gi_check) )
        {
            reset = 1;
            if(render_scene->main_camera.is_changed)
            {
                cl_float* mapped_memory = (cl_float*) clEnqueueMapBuffer( cl_manager->comm_queue,
                                                                          cl_manager->camera_buffer,
                                                                          CL_TRUE,
                                                                          CL_MAP_WRITE,
                                                                          (size_t) 0,
                                                                          sizeof(cl_manager->camera_buffer),
                                                                          0,
                                                                          NULL,
                                                                          NULL,
                                                                          &err
                                                                         );
                CL_context::checkError(err, __FILE__, __LINE__ -1);

                render_scene->main_camera.setBuffer(mapped_memory);

                err = clEnqueueUnmapMemObject(cl_manager->comm_queue, cl_manager->camera_buffer, mapped_memory, 0, NULL, NULL);
                CL_context::checkError(err, __FILE__, __LINE__ -1);

                err = clSetKernelArg(cl_manager->kernel, 2, sizeof(cl_mem), &cl_manager->camera_buffer);
                CL_context::checkError(err, __FILE__, __LINE__ -1);

            }

            if(opts.gi_check != glfw_manager->options.gi_check)
            {
                opts.gi_check = glfw_manager->options.gi_check;
                cl_int check = opts.gi_check;

                err = clSetKernelArg(cl_manager->kernel, 6, sizeof(cl_int), &check);
                CL_context::checkError(err, __FILE__, __LINE__ -1);
            }

            err = clSetKernelArg(cl_manager->kernel, 7, sizeof(cl_int), &reset);
            CL_context::checkError(err, __FILE__, __LINE__ -1);
        }
        else if (reset != 0)
        {
            reset = 0;
            err = clSetKernelArg(cl_manager->kernel, 7, sizeof(cl_int), &reset);
            CL_context::checkError(err, __FILE__, __LINE__ -1);
        }

        // Pass a random seed value.
        err = clSetKernelArg(cl_manager->kernel, 8, sizeof(cl_uint), &seed);
        CL_context::checkError(err, __FILE__, __LINE__ -1);

        /* Switch Buffers. The image written to earlier is now read only. The color value is read from it the averaged with the new color computed and written to the image that
         * previously read-only.
         */
        if(buffer_switch)
        {
            err = clSetKernelArg(cl_manager->kernel, 0, sizeof(cl_manager->image_buffers[0]), &cl_manager->image_buffers[0]);
            CL_context::checkError(err, __FILE__, __LINE__ -1);

            err = clSetKernelArg(cl_manager->kernel, 1, sizeof(cl_manager->image_buffers[1]), &cl_manager->image_buffers[1]);
            CL_context::checkError(err, __FILE__, __LINE__ -1);
        }
        else
        {
            err = clSetKernelArg(cl_manager->kernel, 0, sizeof(cl_manager->image_buffers[1]), &cl_manager->image_buffers[1]);
            CL_context::checkError(err, __FILE__, __LINE__ -1);

            err = clSetKernelArg(cl_manager->kernel, 1, sizeof(cl_manager->image_buffers[0]), &cl_manager->image_buffers[0]);
            CL_context::checkError(err, __FILE__, __LINE__ -1);
        }

    }

    void Renderer::saveImage(unsigned long samples_taken)
    {
        int width = glfw_manager->framebuffer_width;
        int height = glfw_manager->framebuffer_height;
        int bpp = 24; // bits per pixel
        int scanline_width = 3*width;
        unsigned char* img_data = new unsigned char[width*height*3];


        glReadPixels(0, 0, width, height, GL_BGR, GL_UNSIGNED_BYTE, img_data);
        FIBITMAP* bitmap = FreeImage_ConvertFromRawBits(img_data, width, height, scanline_width, bpp, FI_RGBA_RED_MASK, FI_RGBA_GREEN_MASK, FI_RGBA_BLUE_MASK, false);
        std::string fn = this->save_filename + "-" + std::to_string(samples_taken) + "spp" + this->ext;
        FREE_IMAGE_FORMAT extension;

        if(this->ext == ".png")
            extension = FIF_PNG;
        else if(this->ext== ".bmp")
            extension = FIF_BMP;
        else if(this->ext == ".jpeg" || this->ext == ".jpg" || this->ext == ".jpe")
            extension = FIF_JPEG;
        else
            return; // Unsupported type.

        FreeImage_Save(extension, bitmap, fn.data(), 0);
        delete[] img_data;
    }

    void Renderer::getWorkGroupSizes(size_t global_workgroup_size[2], size_t local_workgroup_size[2])
    {
        global_workgroup_size[0] = glfw_manager->framebuffer_width;
        global_workgroup_size[1] = glfw_manager->framebuffer_height;

        size_t total_local_workgroup_size = cl_manager->preferred_workgroup_multiple;
        int x, y;
        float aspect_ratio;
        aspect_ratio = (glfw_manager->framebuffer_width * 1.0f)/ glfw_manager->framebuffer_height;

        //Find the highest multiple workgroup size that kernel can support.
        while(total_local_workgroup_size * 2 < cl_manager->kernel_workgroup_size)
        {
            total_local_workgroup_size *=2;
        }

        x = total_local_workgroup_size;
        y = 1;

        //Split this total local workgroup size into 2 dimensions based on the aspect ratio of Image.
        while( x>y )
        {
            x/= 2;
            y *=  2;
        }

        if( (aspect_ratio > 1) && (x < y) )
        {
            x = x*y;
            y =  x/y;
            x = x/y;
        }

        local_workgroup_size[0] = x;
        local_workgroup_size[1] = y;

        if( local_workgroup_size[0] > cl_manager->target_device.max_workitem_size[0]
                                            ||
            local_workgroup_size[1] > cl_manager->target_device.max_workitem_size[1]
          )
        {
            throw RuntimeError("Number of WorkItems in a dimension of Workgroup exceed Device's Max WorkItem size for that dimension. Aborting...\n", __FILE__, std::to_string(__LINE__));
        }

        if(local_workgroup_size[0]* local_workgroup_size[1] > cl_manager->target_device.wg_size)
        {
            throw RuntimeError("Total number of WorkItems in a Workgroup exceed Device's Max WorkItem size. Aborting...", __FILE__, std::to_string(__LINE__));
        }

    }

}
