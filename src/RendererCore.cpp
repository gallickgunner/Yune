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

#define STB_IMAGE_WRITE_IMPLEMENTATION

#ifdef _WIN32
#include <windows.h>
#endif // defined _WIN32

#include "stb_image_write.h"
#include "RendererCore.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <random>
#include <thread>
#include <chrono>
#include <cmath>
#include <stdint.h>
#include <limits>

namespace yune
{
    std::function<void(const std::string&, const std::string&, const std::string&)> RendererCore::setMessageCb;

    RendererCore::RendererCore(CLManager& cl_manager, GlfwManager& glfw_manager) : cl_manager(cl_manager), glfw_manager(glfw_manager)
    {
        resetValues();
        skip_ticks = 16.666;

        save_editor = false;
        blocks = glm::ivec2(2,2);
        save_samples_ext = ".jpg";

        updateKernelWGSize(true);

        mt_seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        mt_engine = std::mt19937(mt_seed);
        dist = std::uniform_int_distribution<unsigned int>(0,  std::numeric_limits<unsigned int>::max() );
        //ctor
    }

    RendererCore::~RendererCore()
    {
        //dtor
    }

    void RendererCore::setGuiMessageCb(std::function<void(const std::string&, const std::string&, const std::string&)> cb)
    {
        RendererCore::setMessageCb = cb;
    }

    void RendererCore::resetValues()
    {
        buffer_switch = gi_check = render_nextframe = true;
        save_pending = rk_enqueued = ppk_enqueued = false;

        last_time = start_time = samples_taken = save_at_samples = time_passed = 0;
        fps = sum_mspf = mspf_uncapped_avg = mspf_avg = ms_per_ppk = ms_per_rk = 0;
        reset = curr_block = frame_count = 0;
        rk_status = ppk_status = CL_COMPLETE;
    }

    void RendererCore::stop()
    {
        clFinish(cl_manager.comm_queue);
        resetValues();
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, glfw_manager.fbo_ID);
        glDrawBuffer(GL_COLOR_ATTACHMENT0);
        glClear(GL_COLOR_BUFFER_BIT);

        glDrawBuffer(GL_COLOR_ATTACHMENT1);
        glClear(GL_COLOR_BUFFER_BIT);

        glDrawBuffer(GL_COLOR_ATTACHMENT2);
        glClear(GL_COLOR_BUFFER_BIT);

        glDrawBuffer(GL_COLOR_ATTACHMENT3);
        glClear(GL_COLOR_BUFFER_BIT);

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    }

    bool RendererCore::reloadMatFile()
    {
        try
        {
            render_scene.reloadMatFile();
        }
        catch(const std::exception& err)
        {
            setMessageCb(err.what(), "Error loading File!", "");
            return false;
        }

        setMessageCb("File loaded successfully!", "Success!", "");
        return true;
    }

    bool RendererCore::loadScene(std::string path, std::string fn)
    {
        try
        {
            render_scene.loadModel(path, fn);
        }
        catch(const std::exception& err)
        {
            setMessageCb(err.what(), "Error loading File!", "");
            return false;
        }
        setMessageCb("File loaded successfully!", "Success!", "");
        return true;
    }

    void RendererCore::updateKernelWGSize(bool reset)
    {
        if(reset)
        {
            blocks = glm::vec2(2,2);
            rk_lws[0] = 0;
            rk_lws[1] = 0;
            ppk_lws[0] = 0;
            ppk_lws[1] = 0;
        }
        rk_gws[0] = std::ceil((float)glfw_manager.framebuffer_width/blocks.x);
        rk_gws[1] = std::ceil((float)glfw_manager.framebuffer_height/blocks.y);
        ppk_gws[0] = glfw_manager.framebuffer_width;
        ppk_gws[1] = glfw_manager.framebuffer_height;
    }

    bool RendererCore::setup(bool& update_image_buffer, bool& update_vertex_buffer,  bool& update_mat_buffer, bool& update_bvh_buffer, bool do_postproc)
    {
        bool show_error = false;
        this->do_postproc = do_postproc;

        if(update_vertex_buffer)
        {
            if(cl_manager.setupVertexBuffer(render_scene.vert_data, render_scene.scene_size_mb))
               update_vertex_buffer = false;
            else
                show_error = true;
        }

        if(update_mat_buffer)
        {
            if(cl_manager.setupMatBuffer(render_scene.mat_data))
               update_mat_buffer = false;
            else
                show_error = true;
        }

        if(update_bvh_buffer)
        {
            if(cl_manager.setupBVHBuffer(render_scene.bvh.gpu_node_list, render_scene.bvh.bvh_size_mb, render_scene.scene_size_mb))
                update_bvh_buffer = false;
            else
                show_error = true;
        }

        if(update_image_buffer)
        {
            if(glfw_manager.setupGlBuffer() && cl_manager.setupImageBuffers(glfw_manager.rbo_IDs))
                update_image_buffer = false;
            else
                show_error = true;

        }

        /* Pass Scene/Model Data and BVH if present. If not present NULL Buffer will be passed. Since other arguments need to be passed
         * regularly, we pass them in the loop inside start function. Scene and material data remain constant hence passed
         * only once here.
         */
        try
        {
            cl_int err = 0;

            //Set GI Check Arguments
            err = clSetKernelArg(cl_manager.rend_kernel, 8, sizeof(cl_int), &gi_check);
            CLManager::checkError(err, __FILE__, __LINE__ -1);

            //Set Camera Argument
            render_scene.main_camera.setBuffer(&cam_data);
            cl_manager.setupCameraBuffer(&cam_data);
            render_scene.main_camera.is_changed = true;

            //Set Block Arugments
            cl_int bx = blocks.x;
            cl_int by = blocks.y;
            err = clSetKernelArg(cl_manager.rend_kernel, 12, sizeof(cl_int), &bx);
            CLManager::checkError(err, __FILE__, __LINE__ -1);

            err = clSetKernelArg(cl_manager.rend_kernel, 13, sizeof(cl_int), &by);
            CLManager::checkError(err, __FILE__, __LINE__ -1);

            //Set Scene Arguments
            cl_int scene_size = render_scene.vert_data.size();
            cl_int bvh_size = render_scene.bvh.gpu_node_list.size();

            err = clSetKernelArg(cl_manager.rend_kernel, 3, sizeof(cl_int), &scene_size);
            CLManager::checkError(err, __FILE__, __LINE__ -1);

            err = clSetKernelArg(cl_manager.rend_kernel, 4, sizeof(cl_mem), cl_manager.vert_buffer ? &cl_manager.vert_buffer : NULL);
            CLManager::checkError(err, __FILE__, __LINE__ -1);

            err = clSetKernelArg(cl_manager.rend_kernel, 5, sizeof(cl_mem), cl_manager.mat_buffer ? &cl_manager.mat_buffer : NULL);
            CLManager::checkError(err, __FILE__, __LINE__ -1);

            err = clSetKernelArg(cl_manager.rend_kernel, 6, sizeof(cl_int), &bvh_size);
            CLManager::checkError(err, __FILE__, __LINE__ -1);

            err = clSetKernelArg(cl_manager.rend_kernel, 7, sizeof(cl_mem), cl_manager.bvh_buffer ? &cl_manager.bvh_buffer : NULL);
            CLManager::checkError(err, __FILE__, __LINE__ -1);

        }
        catch(const std::exception& err)
        {
            show_error = true;
            setMessageCb(err.what(), "Invalid Kernel Argument!", "");
            return !show_error;
        }
        return !show_error;
    }

    bool RendererCore::enqueueKernels(bool new_gi_check, bool cap_fps)
    {
        bool show_error = false;

        //Setup RBO as the source from where to read pixel data. Set default framebuffer for writing.
        glBindFramebuffer(GL_READ_FRAMEBUFFER, glfw_manager.fbo_ID);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glDrawBuffer(GL_BACK);
        try
        {
            cl_int err = 0;

            //Enqueue Kernel only if previous kernel completed it's execution
            if(rk_status == CL_COMPLETE && ppk_status == CL_COMPLETE)
            {
                //If first curr_block, this means one frame has been processed or it's the initial frame. We start rendering the next frame only if previous frame was blitted through render().
                if(!render_nextframe && cap_fps && curr_block == 0)
                    return true;
                if(curr_block == 0)
                {
                    cl_uint seed = dist(mt_engine);
                    updateRenderKernelArgs(new_gi_check, seed);
                    if(reset == 1)
                        samples_taken = 0;
                    start_time = glfwGetTime();
                    render_nextframe = false;
                }

                //Enqueue Kernel.
                if(!cl_manager.target_device.clgl_event_ext)
                    glFinish();

                err = clEnqueueAcquireGLObjects(cl_manager.comm_queue, 2, cl_manager.image_buffers, 0, NULL, NULL);
                CLManager::checkError(err, __FILE__, __LINE__ -1);
                clFlush(cl_manager.comm_queue);

                err = clSetKernelArg(cl_manager.rend_kernel, 11, sizeof(cl_int), &curr_block);
                CLManager::checkError(err, __FILE__, __LINE__ -1);

                size_t* lws = NULL;
                if(rk_lws[0] > 0 && rk_lws[1] > 0)
                    lws = rk_lws;
                err = clEnqueueNDRangeKernel(cl_manager.comm_queue, // command queue
                                         cl_manager.rend_kernel,    // kernel
                                         2,                         // global work dimensions
                                         NULL,                      // global work offset
                                         rk_gws,                    // global workgroup size
                                         lws,                    // local workgroup size
                                         0,                         // Number of events in wait list.
                                         NULL,                      // Events in wait list
                                         &rk_event
                                        );
                CLManager::checkError(err, __FILE__, __LINE__ -1);

                err = clEnqueueReleaseGLObjects(cl_manager.comm_queue, 2, cl_manager.image_buffers, 0, NULL, NULL);
                CLManager::checkError(err, __FILE__, __LINE__ -1);
                clFlush(cl_manager.comm_queue);
                curr_block++;
                rk_enqueued = true;
            }

            // Check if all blocks have completed execution. If so, then apply post-processing kernel (Mainly tone mapping and Gamma correction)
            if(rk_enqueued)
            {
                clGetEventInfo(rk_event, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof(cl_int), &rk_status, NULL);
                if(rk_status == CL_COMPLETE && curr_block != blocks.x * blocks.y)
                {
                    // Get Profiling info for rendering kernel
                    cl_ulong time_start, time_finish;
                    clGetEventProfilingInfo(rk_event, CL_PROFILING_COMMAND_START, sizeof(time_start), &time_start, NULL);
                    clGetEventProfilingInfo(rk_event, CL_PROFILING_COMMAND_END, sizeof(time_finish), &time_finish, NULL);
                    clReleaseEvent(rk_event);
                    exec_time_rk += (time_finish - time_start)/1000000.0;
                    rk_enqueued = false;
                }
            }

            //If one whole frame has finished, do post-processing if kernel available else store copy of the frame for blitting later
            if(rk_status == CL_COMPLETE && curr_block == blocks.x * blocks.y)
            {
                rk_enqueued = false;
                curr_block = 0;
                //std::this_thread::sleep_for(std::chrono::milliseconds(20));

                // Get Profiling info for rendering kernel
                cl_ulong time_start, time_finish;
                clGetEventProfilingInfo(rk_event, CL_PROFILING_COMMAND_START, sizeof(time_start), &time_start, NULL);
                clGetEventProfilingInfo(rk_event, CL_PROFILING_COMMAND_END, sizeof(time_finish), &time_finish, NULL);
                clReleaseEvent(rk_event);
                exec_time_rk += (time_finish - time_start)/1000000.0;

                 //Enqueue Post Processing kernel if post-proc enabled.
                if(do_postproc)
                {
                    if(!cl_manager.target_device.clgl_event_ext)
                        glFinish();

                    err = clEnqueueAcquireGLObjects(cl_manager.comm_queue, 3, cl_manager.image_buffers, 0, NULL, NULL);
                    CLManager::checkError(err, __FILE__, __LINE__ -1);

                    updatePostProcessingKernelArgs();

                    size_t* lws = NULL;
                    if(rk_lws[0] > 0 && rk_lws[1] > 0)
                        lws = ppk_lws;
                    err = clEnqueueNDRangeKernel(cl_manager.comm_queue,     // command queue
                                                 cl_manager.pp_kernel,      // kernel
                                                 2,                         // global work dimensions
                                                 NULL,                      // global work offset
                                                 ppk_gws,                   // global workgroup size
                                                 lws,                   // local workgroup size
                                                 0,                         // Number of events in wait list.
                                                 NULL,                      // Events in wait list
                                                 &ppk_event                 // Event
                                                );
                    CLManager::checkError(err, __FILE__, __LINE__ -1);

                    err = clEnqueueReleaseGLObjects(cl_manager.comm_queue, 3, cl_manager.image_buffers, 0, NULL, NULL);
                    CLManager::checkError(err, __FILE__, __LINE__ -1);

                    clFlush(cl_manager.comm_queue);
                    ppk_enqueued = true;

                }
                //Else we keep a copy of the latest frame and blit to the default framebuffer (BACK)
                else
                {
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
                    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, glfw_manager.fbo_ID);
                    glDrawBuffer(GL_COLOR_ATTACHMENT3);

                    //Clear previous frame
                    glClear(GL_COLOR_BUFFER_BIT);

                    glBlitFramebuffer(0, 0, glfw_manager.framebuffer_width, glfw_manager.framebuffer_height,
                                      0, 0, glfw_manager.framebuffer_width, glfw_manager.framebuffer_height,
                                      GL_COLOR_BUFFER_BIT,
                                      GL_LINEAR
                                     );
                    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
                    glDrawBuffer(GL_BACK);

                    //Update benchmarks, advance framecount and samples etc.
                    endFrame();
                }
            }

            // If postprocessing was enabled, Check if Post Processing kernel completed execution
            if(ppk_enqueued && do_postproc)
            {
                //double xx = glfwGetTime();
                clGetEventInfo(ppk_event, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof(cl_int), &ppk_status, NULL);
                if(ppk_status == CL_COMPLETE)
                {
                    ppk_enqueued = false;

                    // Get Profiling info for post-processing kernel
                    cl_ulong time_start, time_finish;
                    clGetEventProfilingInfo(ppk_event, CL_PROFILING_COMMAND_START, sizeof(time_start), &time_start, NULL);
                    clGetEventProfilingInfo(ppk_event, CL_PROFILING_COMMAND_END, sizeof(time_finish), &time_finish, NULL);
                    clReleaseEvent(ppk_event);
                    exec_time_ppk += (time_finish - time_start)/1000000.0;

                    //Store a copy of the post-processed frame
                    glBindFramebuffer(GL_FRAMEBUFFER, glfw_manager.fbo_ID);
                    glReadBuffer(GL_COLOR_ATTACHMENT2);
                    glDrawBuffer(GL_COLOR_ATTACHMENT3);

                    //Clear previous frame
                    glClear(GL_COLOR_BUFFER_BIT);

                    glBlitFramebuffer(0, 0, glfw_manager.framebuffer_width, glfw_manager.framebuffer_height,
                                      0, 0, glfw_manager.framebuffer_width, glfw_manager.framebuffer_height,
                                      GL_COLOR_BUFFER_BIT,
                                      GL_LINEAR
                                     );
                    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
                    glDrawBuffer(GL_BACK);

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

                    //  If samples taken is equal to the option specified at which to take a screen shot, save the image.
                    if(save_at_samples > 0 && samples_taken == save_at_samples)
                        show_error |= !saveImage(save_samples_fn, save_samples_ext);

                    //If save button was pressed, save the current image output by the newest kernel execution.
                    if(save_pending)
                    {
                        show_error |= !saveImage(save_fn, save_ext);
                        save_pending = false;
                    }

                    //Update benchmarks, advance framecount and samples etc.
                    endFrame();
                }
            }
        }
        catch(const std::exception& err)
        {
            setMessageCb(err.what(), "Error!", "");
            return false;
        }
        return !show_error;
    }

    void RendererCore::render()
    {
        render_nextframe = true;
        glReadBuffer(GL_COLOR_ATTACHMENT3);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glBlitFramebuffer(0, 0, glfw_manager.framebuffer_width, glfw_manager.framebuffer_height,
                          0, 0, glfw_manager.framebuffer_width, glfw_manager.framebuffer_height,
                          GL_COLOR_BUFFER_BIT,
                          GL_LINEAR
                         );
    }

    void RendererCore::endFrame()
    {
        samples_taken++;
        frame_count++;
        sum_mspf += (glfwGetTime() - start_time);

        //Show ms per frame and per kernel averaged over 1 sec intervals...
        if(glfwGetTime() - last_time >= 1.0)
        {
            mspf_uncapped_avg = sum_mspf * 1000.0 /frame_count;     // Redundant calculation, may come handy later

            mspf_avg = (glfwGetTime() - last_time) * 1000/frame_count;
            ms_per_rk = (float) exec_time_rk / (blocks.x * blocks.y * frame_count);
            ms_per_ppk = (float) exec_time_ppk / frame_count;

            time_passed++;
            fps = frame_count;
            frame_count = 0;
            last_time = glfwGetTime();
            sum_mspf = 0.0f;
            exec_time_rk = exec_time_ppk = 0;
        }
    }

    void RendererCore::updateRenderKernelArgs(bool new_gi_check, cl_uint seed)
    {
        cl_int err = 0, new_reset = 0;
        /* Switch Buffers. The image written to earlier is now read only. The color value is read from it the averaged with the new color
         * computed and written to the image that was previously read-only.
         */

        if(buffer_switch)
        {
            err = clSetKernelArg(cl_manager.rend_kernel, 0, sizeof(cl_manager.image_buffers[0]), &cl_manager.image_buffers[0]);
            CLManager::checkError(err, __FILE__, __LINE__ -1);

            err = clSetKernelArg(cl_manager.rend_kernel, 1, sizeof(cl_manager.image_buffers[1]), &cl_manager.image_buffers[1]);
            CLManager::checkError(err, __FILE__, __LINE__ -1);
        }
        else
        {
            err = clSetKernelArg(cl_manager.rend_kernel, 0, sizeof(cl_manager.image_buffers[1]), &cl_manager.image_buffers[1]);
            CLManager::checkError(err, __FILE__, __LINE__ -1);

            err = clSetKernelArg(cl_manager.rend_kernel, 1, sizeof(cl_manager.image_buffers[0]), &cl_manager.image_buffers[0]);
            CLManager::checkError(err, __FILE__, __LINE__ -1);
        }

        // Set the reset flag to 1 when Camera changes orientation. This configures kernel to write the current color instead of averaging it with the previous one.
        if(render_scene.main_camera.is_changed)
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
            CLManager::checkError(err, __FILE__, __LINE__ -1);

            render_scene.main_camera.setBuffer(mapped_memory);

            err = clEnqueueUnmapMemObject(cl_manager.comm_queue, cl_manager.camera_buffer, mapped_memory, 0, NULL, NULL);
            CLManager::checkError(err, __FILE__, __LINE__ -1);

            err = clSetKernelArg(cl_manager.rend_kernel, 2, sizeof(cl_mem), &cl_manager.camera_buffer);
            CLManager::checkError(err, __FILE__, __LINE__ -1);
        }

        // Pass GI check value. Change the reset value to 1.
        if(gi_check != new_gi_check)
        {
            gi_check = new_gi_check;
            cl_int check = gi_check;

            err = clSetKernelArg(cl_manager.rend_kernel, 8, sizeof(cl_int), &check);
            CLManager::checkError(err, __FILE__, __LINE__ -1);
            new_reset = 1;
        }

        // Pass reset value. If there was no change in reset value, no need to pass again.
        if (reset != new_reset)
        {
            reset = new_reset;
            err = clSetKernelArg(cl_manager.rend_kernel, 9, sizeof(cl_int), &reset);
            CLManager::checkError(err, __FILE__, __LINE__ -1);
        }

        // Pass a random seed value.
        err = clSetKernelArg(cl_manager.rend_kernel, 10, sizeof(cl_uint), &seed);
        CLManager::checkError(err, __FILE__, __LINE__ -1);
    }

    void RendererCore::updatePostProcessingKernelArgs()
    {
        cl_int err = 0;

        //Pass Current frame
        if(buffer_switch)
            err = clSetKernelArg(cl_manager.pp_kernel, 0, sizeof(cl_manager.image_buffers[0]), &cl_manager.image_buffers[0]);
        else
            err = clSetKernelArg(cl_manager.pp_kernel, 0, sizeof(cl_manager.image_buffers[1]), &cl_manager.image_buffers[1]);
        CLManager::checkError(err, __FILE__, __LINE__ -1);

        //Pass Previous frame
        if(buffer_switch)
            err = clSetKernelArg(cl_manager.pp_kernel, 1, sizeof(cl_manager.image_buffers[1]), &cl_manager.image_buffers[1]);
        else
            err = clSetKernelArg(cl_manager.pp_kernel, 1, sizeof(cl_manager.image_buffers[0]), &cl_manager.image_buffers[0]);
        CLManager::checkError(err, __FILE__, __LINE__ -1);

        //Pass Output Image to contain tonemapped data
        err = clSetKernelArg(cl_manager.pp_kernel, 2, sizeof(cl_manager.image_buffers[2]), &cl_manager.image_buffers[2]);
        CLManager::checkError(err, __FILE__, __LINE__ -1);

        //Pass Reset Value. This tells the kernel if previous frame is available for calculating exposure value
        err = clSetKernelArg(cl_manager.pp_kernel, 3, sizeof(cl_int), &reset);
        CLManager::checkError(err, __FILE__, __LINE__ -1);
    }

    bool RendererCore::saveImage(std::string save_fn, std::string save_ext)
    {
        int width = glfw_manager.framebuffer_width;
        int height = glfw_manager.framebuffer_height;

        int stride = (width % 4) + (width * 3);
        bool status = false;
        stbi_flip_vertically_on_write(1);

        if(save_ext == ".hdr")
        {
            float* img_data = new float[stride*height*3];
            glReadPixels(0, 0, width, height, GL_RGB, GL_FLOAT, img_data);
            status = stbi_write_hdr(save_fn.c_str(), width, height, 3, img_data);
            delete[] img_data;
        }
        else
        {
            unsigned char* img_data = new unsigned char[stride*height*3];
            if(save_editor && !save_pending)
            {
                glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
                glReadBuffer(GL_BACK);
            }
            else
                glReadBuffer(GL_COLOR_ATTACHMENT3);
            glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, img_data);

            if(save_ext == ".png")
                status = stbi_write_png(save_fn.c_str(), width, height, 3, img_data, stride);
            else if(save_ext == ".jpg")
                status = stbi_write_jpg(save_fn.c_str(), width, height, 3, img_data, 100);

            delete[] img_data;
        }
        if(!status)
            setMessageCb("Error Saving Image. Please make sure a valid save file name is provided", "Error!", "");
        return status;
    }
}
