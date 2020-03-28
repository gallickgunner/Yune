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

#include "RendererGUI.h"
#include "Scene.h"
#include "imgui_internal.h"
#include "glm/vec2.hpp"
#include "glm/gtc/type_ptr.hpp"

#include <functional>
#include <iostream>
#include <chrono>
#include <thread>
#include <exception>

namespace yune {

    std::string RendererGUI::mb_msg;
    std::string RendererGUI::mb_title;
    std::string RendererGUI::mb_log;

    RendererGUI::RendererGUI(int window_width, int window_height) : glfw_manager(window_width, window_height), cl_manager(), renderer(this->cl_manager, this->glfw_manager)
    {
        do_postproc = gi_check = misc_settings_shown = scene_info_shown = benchmark_shown = true;
        update_mat_buffer = update_vertex_buffer = update_bvh_buffer  = is_fullscreen = renderer_start = false;
        cap_fps = update_image_buffer = true;
        bvh_bins = 20;
        input_fn[0] = '\0';
        benchmark_wheight = 0;

        selected_size = 3;
        supported_sizes.push_back({600, 800});
        supported_sizes.push_back({640, 480});
        supported_sizes.push_back({800, 600});
        supported_sizes.push_back({1024, 768});
        supported_sizes.push_back({1280, 720});
        supported_sizes.push_back({1280, 1024});
        supported_sizes.push_back({1600, 900});
        supported_sizes.push_back({1920, 1080});

        supported_exts.push_back(".png");
        supported_exts.push_back(".jpg");
        supported_exts.push_back(".hdr");
    }

    RendererGUI::~RendererGUI()
    {
        //dtor
    }

    void RendererGUI::startFrame()
    {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void RendererGUI::renderFrame()
    {
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    void RendererGUI::setMessage(const std::string& msg, const std::string& title, const std::string& log)
    {
        RendererGUI::mb_msg = msg;
        RendererGUI::mb_title = title;
        RendererGUI::mb_log = log;
    }

    bool RendererGUI::setup()
    {
        glfw_manager.setGuiMessageCb(std::bind( &(RendererGUI::setMessage), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
        cl_manager.setGuiMessageCb(std::bind( &(RendererGUI::setMessage), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
        renderer.setGuiMessageCb(std::bind( &(RendererGUI::setMessage), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        /* Since GLFW manager don't have access to camera variable, pass Camera's function as callback.
         *  This function is called when Camera is updated through mouse/keyboard.
         */
        glfw_manager.setCameraUpdateCallback(std::bind( &(renderer.render_scene.main_camera.setOrientation),
                                                    &(renderer.render_scene.main_camera),
                                                    std::placeholders::_1,
                                                    std::placeholders::_2,
                                                    std::placeholders::_3)
                                         );
        cl_manager.setup();
        bool kernel_loaded = false;
        kernel_loaded = cl_manager.createRenderProgram("udpt.cl", "./kernels/legacy/udpt-primitives.cl", false);
        kernel_loaded &= cl_manager.createPostProcProgram("tonemap.cl", "./kernels/post-proc/tonemap.cl", false);
        if(kernel_loaded)
        {
            RendererGUI::mb_title = "Success!";
            RendererGUI::mb_msg = "Kernels loaded successfully!";
        }
        else
        {
            RendererGUI::mb_title = "Error!";
            RendererGUI::mb_msg = "Failed to load default kernels. Either someone modified them or you are missing default files \"./kernels/legacy/udpt-primitives.cl\" and \"./kernels/post-proc/tonemap.cl\"";
        }
        return true;
    }

    void RendererGUI::run()
    {
        bool show_message = false;
        unsigned long time_passed = 0;
        double start_time = 0, skip_ticks = 16.66666;

        show_message = setup();
        glfw_manager.showWindow();
        renderer.updateKernelWGSize();

        std::cout << "\nStarting Renderer..." << std::endl;
        while(!glfwWindowShouldClose(glfw_manager.window))
        {
            /* We need to call enqueueKernels method of RendererCore as it enqueues kernels in blocks of a single image. If gpu has already finished processing
             * one whole frame, it can be rendered through render() call. If last frame is not rendered, we don't enqueue kernels for the next frame.
             */
            if(renderer_start)
            {
                if(!renderer.enqueueKernels(gi_check, cap_fps))
                {
                    show_message = true;
                    renderer_start = false;
                }
            }

            //Lock GUI frame rate to 16.66 ms. This ensures that we don't render GUI at very high fps (1k+, only possible if kernel has very low execution time).
            if( (glfwGetTime() - start_time) * 1000 < skip_ticks)
                continue;

            //Start Drawing a new frame
            start_time = glfwGetTime();

            glClear(GL_COLOR_BUFFER_BIT);

            //Render last frame processed by GPU.
            if(renderer_start)
                renderer.render();

            startFrame();
            show_message |= showMenu();

            if(benchmark_shown)
                showBenchmarkWindow(time_passed);

            if(scene_info_shown)
                showSceneInfo();

            if(misc_settings_shown)
                showMiscSettings();

            if(show_message)
            {
                show_message = false;
                ImGui::OpenPopup(mb_title.c_str());
            }
            showMessageBox(mb_title, mb_msg, mb_log);
            renderFrame();
            glfwSwapBuffers(glfw_manager.window);
            glfwPollEvents();
        }
    }

    bool RendererGUI::showMenu()
    {
        bool open_obj = false, save_fildialog = false, open_rk = false, open_ppk = false, open_about = false, open_usage = false, show_message = false;
        if(ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Load OBJ", NULL, false, !renderer_start))
                    open_obj = true;

                if (ImGui::MenuItem("Load Render Kernel", NULL, false, !renderer_start))
                    open_rk = true;

                if (ImGui::MenuItem("Load Post-Proc Kernel", NULL, false, !renderer_start))
                    open_ppk = true;

                if(ImGui::MenuItem("Reload Render Kernel", NULL, false, !cl_manager.rk_file.empty() && !renderer_start))
                {
                    show_message = true;
                    cl_manager.createRenderProgram("", "", true);
                }

                if(ImGui::MenuItem("Reload Post-Proc Kernel", NULL, false, !cl_manager.ppk_file.empty() && !renderer_start))
                {
                    show_message = true;
                    cl_manager.createPostProcProgram("", "", true);
                }

                if(ImGui::MenuItem("Reload Material File", NULL, false, !renderer.render_scene.mat_file.empty() && !renderer_start))
                {
                    show_message = true; //Either success or failure.
                    if(renderer.reloadMatFile())
                        update_mat_buffer = true;
                }

                if(ImGui::MenuItem("Save Image", NULL, false, renderer_start))
                    save_fildialog = true;

                ImGui::Separator();
                if(ImGui::MenuItem("Start", NULL, &renderer_start, !cl_manager.rk_file.empty()))
                {
                    if(renderer_start && !renderer.setup(update_image_buffer, update_vertex_buffer, update_mat_buffer, update_bvh_buffer, !cl_manager.ppk_file.empty() && do_postproc))
                    {
                        renderer_start = false;
                        show_message = true;
                    }
                    else if(!renderer_start)
                        renderer.stop();
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Settings"))
            {
                if(ImGui::MenuItem("Benchmark", NULL, &benchmark_shown))
                {
                    if(!benchmark_shown)
                        benchmark_wheight = 25;
                }
                ImGui::MenuItem("Scene Info", NULL, &scene_info_shown);
                ImGui::MenuItem("Misc Settings", NULL, &misc_settings_shown);

                if(ImGui::BeginMenu("Window Size", !renderer_start))
                {
                    for(int i = 0; i < supported_sizes.size(); i++)
                    {
                        std::string win_size = std::to_string(supported_sizes[i].width) + "x" + std::to_string(supported_sizes[i].height);
                        if(ImGui::MenuItem(win_size.c_str(), NULL, selected_size == i, !is_fullscreen))
                        {
                            selected_size = i;
                            update_image_buffer = true;
                            glfwSetWindowSize(glfw_manager.window, supported_sizes[i].width, supported_sizes[i].height);
                            renderer.updateKernelWGSize();
                        }
                    }
                    ImGui::Separator();
                    if(ImGui::MenuItem("FullScreen", NULL, &is_fullscreen))
                    {
                        if(is_fullscreen)
                        {
                            selected_size = -1;
                            glfwSetWindowMonitor(glfw_manager.window, glfw_manager.prim_monitor, 0, 0, glfw_manager.mode->width, glfw_manager.mode->height, glfw_manager.mode->refreshRate);
                        }
                        else
                        {
                            selected_size = 3;
                            glfwSetWindowMonitor(glfw_manager.window, NULL, 200, 200, supported_sizes[3].width, supported_sizes[3].height, glfw_manager.mode->refreshRate);
                        }
                        renderer.updateKernelWGSize();
                        update_image_buffer = true;
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Help"))
            {
                if(ImGui::MenuItem("About"))
                    open_about = true;
                if(ImGui::MenuItem("Usage"))
                    open_usage = true;
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        if(open_obj)
            ImGui::OpenPopup("Open OBJ File");

        if(open_rk)
            ImGui::OpenPopup("Open Rendering Kernel File");

        if(open_ppk)
            ImGui::OpenPopup("Open Post-Processing Kernel File");

        if(save_fildialog)
            ImGui::OpenPopup("Save Image");

        if(file_dialog.showFileDialog("Open OBJ File", imgui_addons::ImGuiFileBrowser::DialogMode::OPEN, ImVec2(700, 310), ".obj,.rtt"))
        {
            show_message = true;
            renderer.loadScene(file_dialog.selected_path, file_dialog.selected_fn);
            update_vertex_buffer = true;
            update_mat_buffer = true;
            if(load_bvh)
                update_bvh_buffer = true;
        }

        if(file_dialog.showFileDialog("Open Rendering Kernel File", imgui_addons::ImGuiFileBrowser::DialogMode::OPEN, ImVec2(700, 310), ".cl"))
        {
            show_message = true;
            cl_manager.createRenderProgram(file_dialog.selected_fn, file_dialog.selected_path, false);
        }

        if(file_dialog.showFileDialog("Open Post-Processing Kernel File", imgui_addons::ImGuiFileBrowser::DialogMode::OPEN, ImVec2(700, 310), ".cl"))
        {
            show_message = true;
            cl_manager.createPostProcProgram(file_dialog.selected_fn, file_dialog.selected_path, false);
        }

        if(file_dialog.showFileDialog("Save Image", imgui_addons::ImGuiFileBrowser::DialogMode::SAVE, ImVec2(700, 310), ".png,.jpg,.hdr"))
        {
            renderer.save_pending = true;
            renderer.save_fn = file_dialog.selected_path;
            renderer.save_ext = file_dialog.ext;
        }

        if(open_about)
            ImGui::OpenPopup("About");
        showMessageBox("About", "Yune is a personal project and also an educational raytracer/pathtracer. It's designed for young researchers/programmers who want "
                       "to get into writing their own raytracers/pathtracers but don't want to dabble into boilerplate code and setting up contexts, etc.\n\nNow you just "
                       "need to modify the kernel file which contains the actual rendering logic and code and you can view the results without recompiling the whole project.");

        if(open_usage)
            ImGui::OpenPopup("Usage");
        showMessageBox("Usage", "Load OpenCL kernels for rendering and post-processing (optional) from File menu. You can also load simple obj files (check github for details). "
                       "You can change window size from Settings menu, however this is only available when not rendering. Misc Settings provide other options like setting View-to-World Matrix, "
                       "BVH settings, It also provides settings for kernel Work group sizes. Some of them are only available when not rendering.\n\nFor a simple usecase you only need "
                       "to load kernel for rendering and press start to see the results. Informatin regarding kernel is present on Github, so do check that.");

        return show_message;

    }

    void RendererGUI::showBenchmarkWindow(unsigned long time_passed)
    {
        ImGuiIO& io = ImGui::GetIO();
        ImVec2 window_pos(io.DisplaySize.x - 10, 35);

        ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, ImVec2(1,0));
        ImGui::SetNextWindowSize(ImVec2(270, 0));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.1,0.1,0.1,0.85));
        if (ImGui::Begin("Benchmark##window", &benchmark_shown, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav))
        {
            ImGui::Text("Rendering Kernel");
            ImGui::SameLine();
            ImGui::SetCursorPosX(140);
            ImGui::Text(": ");
            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() - ImGui::GetStyle().ItemSpacing.x);
            ImGui::TextWrapped("%s", cl_manager.rk_file.c_str());

            ImGui::Text("Post-Proc Kernel");
            ImGui::SameLine();
            ImGui::SetCursorPosX(140);
            ImGui::Text(": ");
            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() - ImGui::GetStyle().ItemSpacing.x);
            ImGui::TextWrapped("%s", cl_manager.ppk_file.c_str());

            ImGui::Text("FPS");
            ImGui::SameLine();
            showHelpMarker("This is the number of frames processed by the GPU in one second.");
            ImGui::SameLine();
            ImGui::SetCursorPosX(140);
            ImGui::Text(": %d", renderer.fps);

            ImGui::Text("ms/frame");
            ImGui::SameLine();
            showHelpMarker("This is the time taken by the GPU to render one whole frame. By default the image is broken in 4 blocks. Roughly equal to 4 times ms/rk. Additional overhead due to kernel launch, blitting etc.");
            ImGui::SameLine();
            ImGui::SetCursorPosX(140);
            ImGui::Text(": %.2f ms", renderer.mspf_avg);

            ImGui::Text("ms/rk");
            ImGui::SameLine();
            showHelpMarker("This is the average time taken by a single kernel instance. By default the image is broken in 4 blocks and a kernel is launched for each block.");
            ImGui::SameLine();
            ImGui::SetCursorPosX(140);
            ImGui::Text(": %.2f ms", renderer.ms_per_rk);

            ImGui::Text("ms/ppk");
            ImGui::SameLine();
            showHelpMarker("This is the time taken by the post-processing kernel if loaded.");
            ImGui::SameLine();
            ImGui::SetCursorPosX(140);
            ImGui::Text(": %.2f ms", renderer.ms_per_ppk);

            ImGui::Text("Render Time");
            ImGui::SameLine();
            ImGui::SetCursorPosX(140);
            ImGui::Text(": %lu sec", renderer.time_passed);

            ImGui::Text("Samples Taken");
            ImGui::SameLine();
            ImGui::SetCursorPosX(140);
            ImGui::Text(": %lu spp", renderer.samples_taken);

            benchmark_wheight = 35 + ImGui::GetWindowHeight();
        }
        ImGui::End();
        ImGui::PopStyleColor();
    }

    void RendererGUI::showSceneInfo()
    {
        ImGuiIO& io = ImGui::GetIO();
        ImVec2 window_pos(io.DisplaySize.x - 10, benchmark_wheight + 10);

        ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, ImVec2(1,0));
        ImGui::SetNextWindowSize(ImVec2(270, 0));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.1,0.1,0.1,0.85));

        Scene& scene = renderer.render_scene;
        if (ImGui::Begin("Scene Info##window", &scene_info_shown, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav))
        {
            ImGui::Text("OBJ File");
            ImGui::SameLine();
            ImGui::SetCursorPosX(140);
            ImGui::Text(": ");
            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() - ImGui::GetStyle().ItemSpacing.x);
            ImGui::TextWrapped("%s", renderer.render_scene.scene_file.c_str());

            ImGui::Text("Material File");
            ImGui::SameLine();
            ImGui::SetCursorPosX(140);
            ImGui::Text(": ");
            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() - ImGui::GetStyle().ItemSpacing.x);
            ImGui::TextWrapped("%s", renderer.render_scene.mat_filename.c_str());

            ImGui::Text("No. Triangles");
            ImGui::SameLine();
            ImGui::SetCursorPosX(140);
            ImGui::Text(": %d", renderer.render_scene.num_triangles);

            ImGui::Text("Scene Size");
            ImGui::SameLine();
            showHelpMarker("Total Size in MegaBytes taken by array of structure of triangles sent to GPU");
            ImGui::SameLine();
            ImGui::SetCursorPosX(140);
            if(scene.scene_size_mb < 1.0f)
                ImGui::Text(": %.2f KB", scene.scene_size_kb);
            else
                ImGui::Text(": %.2f MB", scene.scene_size_mb);

            ImGui::Text("BVH Size");
            ImGui::SameLine();
            ImGui::SetCursorPosX(140);
            if(scene.scene_size_mb < 1.0f)
                ImGui::Text(": %.2f KB", scene.bvh.bvh_size_kb);
            else
                ImGui::Text(": %.2f MB", scene.bvh.bvh_size_mb);

            ImGui::End();
        }
        ImGui::PopStyleColor();
    }

    void RendererGUI::showMiscSettings()
    {
        ImGuiStyle& style = ImGui::GetStyle();
        ImVec2 window_pos(10, 35);

        ImGui::SetNextWindowSize(ImVec2(330,500), ImGuiCond_Appearing);
        ImGui::SetNextWindowSizeConstraints(ImVec2(330, 0), ImVec2(330, glfw_manager.framebuffer_height/1.3f));
        ImGui::SetNextWindowPos(window_pos, ImGuiCond_Once, ImVec2(0,0));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.15,0.15,0.15,0.85));
        if (ImGui::Begin("Misc Settings##window", &misc_settings_shown, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing))
        {
            Camera& cam = renderer.render_scene.main_camera;
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(style.ItemSpacing.x, style.ItemSpacing.y * 2.0));
            if(ImGui::CollapsingHeader("Camera Settings", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::PushItemWidth(120);
                if(ImGui::DragFloat("Y-FOV", &cam.y_FOV, 0.5, 10.0, 170.0))
                    cam.updateViewPlaneDist();
                ImGui::DragFloat("Rotation Speed", &cam.rotation_speed, 0.5, 0);
                ImGui::DragFloat("Movement Speed", &cam.move_speed, 0.5, 0);
                ImGui::PopItemWidth();
                ImGui::SetCursorPosY(ImGui::GetCursorPosY()+5);

                ImGui::SetCursorPosX(ImGui::GetWindowWidth()/2.0 - ImGui::CalcTextSize("View-To-World Matrix").x/2.0 - ImGui::GetFrameHeight());
                ImGui::Text("View-To-World Matrix");
                ImGui::SameLine();
                showHelpMarker("Set the Columns of view-to-world matrix. The columns represent the basis vectors, side, up, -lookAt (right-handed) in order. The 4th Column represents the translation vector i.e. the Camera Origin. ");

                ImGui::PushItemWidth(260);
                float* mat = glm::value_ptr(cam.view2world_mat);
                ImGui::DragFloat4("C1-X", mat, 0.02, -1.0, 1.0);
                ImGui::DragFloat4("C2-Y", &mat[4], 0.02, -1.0, 1.0);
                ImGui::DragFloat4("C3-Z", &mat[8], 0.02, -1.0, 1.0);
                ImGui::DragFloat4("C4-T", &mat[12], 0.02);
                ImGui::PopItemWidth();

                ImGui::SetCursorPosX(ImGui::GetWindowWidth()/2.0 - (ImGui::CalcTextSize("Set Matrix").x + ImGui::CalcTextSize("Reset Camera").x + style.ItemSpacing.x)/2.0);
                if(ImGui::Button("Set Matrix"))
                    cam.updateViewMatrix();
                ImGui::SameLine();
                if(ImGui::Button("Reset Camera"))
                    cam.resetCamera();
            }

            //Renderer Settings
            if(ImGui::CollapsingHeader("Renderer Settings", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Checkbox("Enable Post-Processing", &do_postproc);
                ImGui::Checkbox("Global Illumination", &gi_check);
                ImGui::SameLine();
                showHelpMarker("A handy check for toggling GI. This will be passed on to the kernel, so make sure the kernel has the argument for it.");
                ImGui::Checkbox("60-FPS Cap", &cap_fps);
                ImGui::SameLine();
                showHelpMarker("Checking it causes FPS to cap to 60. Only useful if your raytracer/pathtracer gives FPS greater than 60 (Goodluck with that xD).");

                unsigned long min_samples = 0;
                ImGui::PushItemWidth(120);
                ImGui::DragScalar("Save At Samples", ImGuiDataType_U32, &renderer.save_at_samples, 0.5, &min_samples);
                if(ImGui::InputText("Save Filename", input_fn, 256, ImGuiInputTextFlags_AutoSelectAll))
                    renderer.save_samples_fn = std::string(input_fn);
                ImGui::PopItemWidth();
                renderExtBox();
                ImGui::Checkbox("Save Image as Screencap", &renderer.save_editor);
                ImGui::SameLine();
                showHelpMarker("Checking this causes \"Save At Samples\" to work as screen capture and saves Editor to the image as well.");
            }

            //BVH Settings
            if(ImGui::CollapsingHeader("BVH Settings", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen))
            {
                if(renderer_start)
                {
                    ImGui::PushItemFlag(ImGuiItemFlags_Disabled, renderer_start);
                    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
                }

                ImGui::Checkbox("Load BVH", &load_bvh);
                ImGui::PushItemWidth(120);
                ImGui::DragInt("Bins", &bvh_bins, 0.5, 1, 256);
                ImGui::SameLine();
                showHelpMarker("Set the number of bins/buckets to use in SAH based BVH construction. Set Values lower  than 3 to use the Median Splitting Technique.");
                ImGui::PopItemWidth();
                if(ImGui::Button("Create BVH"))
                {
                    renderer.render_scene.loadBVH(bvh_bins);
                    update_bvh_buffer = true;
                }
                if(renderer_start)
                {
                    ImGui::PopItemFlag();
                    ImGui::PopStyleVar();
                }
            }

            //Kernel Settings
            if(ImGui::CollapsingHeader("Kernel Settings", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen))
            {
                if(renderer_start)
                {
                    ImGui::PushItemFlag(ImGuiItemFlags_Disabled, renderer_start);
                    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
                }
                ImGui::PushItemWidth(125);

                if(ImGui::DragInt("##Render Kernel BlocksX", &renderer.blocks.x, 0.2, 1, 100, "Blocks in X: %d"))
                    renderer.updateKernelWGSize();
                ImGui::SameLine();
                if(ImGui::DragInt("##Render Kernel BlocksY", &renderer.blocks.y, 0.2, 1, 100, "Blocks in Y: %d"))
                    renderer.updateKernelWGSize();
                ImGui::SameLine();
                showHelpMarker("You can divide the Image in blocks to allow the same rendering kernel to launch multiple times for a single frame. "
                               "This decreases overall fps but also decreases execution time for 1 kernel instance which can be useful to avoid laggy GUI. Default 2x2=4");

                size_t max_val, min_val;
                min_val = 0;
                max_val = std::numeric_limits<unsigned long>::max();

                ImGui::DragScalar("##RKGWSX", ImGuiDataType_U32, &renderer.rk_gws[0], 0.2, &min_val, &max_val, "RK GWS X: %d");
                ImGui::SameLine();
                ImGui::DragScalar("##RKGWSY", ImGuiDataType_U32, &renderer.rk_gws[1], 0.2, &min_val, &max_val, "RK GWS Y: %d");
                ImGui::SameLine();
                showHelpMarker("Global Workgroup Size for each Rendering Kernel instance in X and Y");

                ImGui::DragScalar("##PPKGWSX", ImGuiDataType_U32, &renderer.ppk_gws[0], 0.2, &min_val, &max_val, "PPK GWS X: %d");
                ImGui::SameLine();
                ImGui::DragScalar("##PPKGWSY", ImGuiDataType_U32, &renderer.ppk_gws[1], 0.2, &min_val, &max_val, "PPK GWS Y: %d");
                ImGui::SameLine();
                showHelpMarker("Global Workgroup Size for the Post-Processing Kernel in X and Y");

                ImGui::DragScalar("##RKLWSX", ImGuiDataType_U32, &renderer.rk_lws[0], 0.2, &min_val, &max_val, "RK LWS X: %d");
                ImGui::SameLine();
                ImGui::DragScalar("##RKLWSY", ImGuiDataType_U32, &renderer.rk_lws[1], 0.2, &min_val, &max_val, "RK LWS Y: %d");
                ImGui::SameLine();
                showHelpMarker("Local Workgroup Size for each Rendering Kernel instance in X and Y. Set to 0 to let OpenCL find a size automatically.");

                ImGui::DragScalar("##PPKLWSX", ImGuiDataType_U32, &renderer.ppk_lws[0], 0.2, &min_val, &max_val, "PPK LWS X: %d");
                ImGui::SameLine();
                ImGui::DragScalar("##PPKLWSY", ImGuiDataType_U32, &renderer.ppk_lws[1], 0.2, &min_val, &max_val, "PPK LWS Y: %d");
                ImGui::SameLine();
                showHelpMarker("Local Workgroup Size for the Post-Processing Kernel in X and Y. Set to 0 to let OpenCL find a size automatically.");

                ImGui::SetCursorPosX(ImGui::GetWindowWidth()/2.0 - ImGui::CalcTextSize("Reset Kernel Settings").x/2.0);
                if(ImGui::Button("Reset Kernel Settings"))
                    renderer.updateKernelWGSize(true);

                if(renderer_start)
                {
                    ImGui::PopItemFlag();
                    ImGui::PopStyleVar();
                }
                ImGui::PopItemWidth();
            }
            ImGui::PopStyleVar();
        }
        ImGui::End();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }

    void RendererGUI::renderExtBox()
    {
        ImGui::PushItemWidth(120);
        if(ImGui::BeginCombo("Extension##FileTypes", renderer.save_samples_ext.c_str()))
        {
            for(int i = 0; i < supported_exts.size(); i++)
            {
                if(ImGui::Selectable(supported_exts[i].c_str(), renderer.save_samples_ext == supported_exts[i]))
                    renderer.save_samples_ext = supported_exts[i];
            }
            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();
    }

    void RendererGUI::showMessageBox(std::string title, std::string msg, std::string log)
    {
        ImGuiStyle& style = ImGui::GetStyle();
        int wrap_size;
        if(log.empty())
        {
            wrap_size = 380;
            ImGui::SetNextWindowSize(ImVec2(400, 0), ImGuiCond_Appearing);
        }
        else
        {
            wrap_size = 460;
            ImGui::SetNextWindowSize(ImVec2(500, 0), ImGuiCond_Appearing);
        }

        ImGui::SetNextWindowSizeConstraints(ImVec2(400, 0), ImVec2(500, 400));
        if (ImGui::BeginPopupModal(title.c_str(), NULL, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImVec2 text_size = ImGui::CalcTextSize(msg.c_str(), NULL, true, wrap_size);
            ImGui::PushTextWrapPos(wrap_size);
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (ImGui::GetWindowWidth() - text_size.x)/2.0f);
            ImGui::Text(msg.c_str());
            ImGui::PopTextWrapPos();

            if(!log.empty())
            {
                ImVec2 text_size = ImGui::CalcTextSize(log.c_str(), NULL, true, wrap_size);
                float cw_width = text_size.x + style.ScrollbarSize + style.WindowPadding.x * 2.0f;
                ImGui::BeginChild("Error Log", ImVec2(cw_width, std::min(500.0f, text_size.y)), true);
                ImGui::PushTextWrapPos(wrap_size);
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (cw_width - style.ScrollbarSize - text_size.x)/2.0f - style.WindowPadding.x);
                ImGui::TextUnformatted(log.c_str());
                ImGui::PopTextWrapPos();
                ImGui::EndChild();
            }

            ImGui::Separator();
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2);
            ImGui::SetCursorPosX(ImGui::GetWindowWidth()/2.0 - ImGui::GetFrameHeight()/2);
            if (ImGui::Button("OK"))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
    }

    void RendererGUI::showHelpMarker(std::string desc)
    {
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::TextUnformatted(desc.c_str());
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }
}
