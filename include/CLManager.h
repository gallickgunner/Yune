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

#ifndef CLMANAGER_H
#define CLMANAGER_H

#include "BVH.h"
#include "CL_headers.h"
#include "glad/glad.h"

#include <string>
#include <vector>
#include <functional>

namespace yune
{

    /** \brief This class manages the OpenCL context and Buffers. The class manages the releasing of memory objects, kernels etc in
     *  RAII fashion. This class is intended for 1 time use only i.e. no more than one instance.
     */
    class CLManager
    {
        public:
            CLManager();    /**< Default Constructor. */
            ~CLManager();   /**< Default Destructor. */


            void setup();   /**<  Setup OpenCL platforms, devices and context*/

            /** \brief Create OpenCL Rendering program and setup kernel.
             *
             * \param[in] fn     The file name containing the kernel for Path tracer/Ray-tracer or any similar Rendering tehcnique.
             * \param[in] path   The full path of the file name above
             * \return True if the function succeeds, else false. Error message will be stored in CLManager::error_msg
             */
            bool createRenderProgram(std::string fn, std::string path, bool reload);

            /** \brief Create OpenCL Rendering program and setup kernel.
             *
             * \param[in] fn     The file name containing the kernel for Post-Processing/Tonemapping
             * \param[in] path   The full path of the file name above
             * \return True if the function succeeds, else false. Error message will be stored in CLManager::error_msg
             */
            bool createPostProcProgram(std::string fn, std::string path, bool reload);

            /** \brief Display the Error message for the error code.
             *
             * \param err_code The error code returned by OpenCL functions.
             * \param filename The file name where the error occurred. Used for pin-pointing where the exception occurred.
             * \param line_number The line number at which the error occurred. Used for pin-pointing where the exception occurred.
             */
            static void checkError(cl_int err_code, std::string filename, int line_number);

            /** \brief Set the callback function to set messages shown by GUI incase of any event
             */
            void setGuiMessageCb(std::function<void(const std::string&, const std::string&, const std::string&)>);


            //Setup Buffer Objects
            void setupCameraBuffer(Cam* cam_data);
            bool setupImageBuffers(GLuint rbo_IDs[]);
            bool setupBVHBuffer(std::vector<BVHNodeGPU>& bvh_data, float bvh_size, float scene_size);
            bool setupVertexBuffer(std::vector<TriangleGPU>& vert_data, float scene_size);
            bool setupMatBuffer(std::vector<Material>& mat_data);

            std::string rk_file, rk_file_path, rk_name, ppk_file, ppk_file_path, ppk_name, rk_compiler_opts, ppk_compiler_opts;

        private:
            class Device
            {
                public:
                    Device();
                    ~Device();
                    void loadInfo(cl_device_id dev_id);
                    void displayInfo(int i);

                    cl_device_id device_id;
                    std::string name;
                    std::string type;
                    std::string device_ver;
                    std::string device_openclC_ver;
                    std::string driver_ver;
                    std::string ext;
                    std::vector<size_t> max_workitem_size;
                    cl_device_type device_type;
                    cl_bool availability;
                    cl_device_fp_config fp_support;
                    cl_uint compute_units;
                    size_t wg_size;
                    cl_ulong global_mem_size;
                    cl_ulong local_mem_size;
                    cl_ulong constant_mem_size;
                    bool clgl_event_ext;
                    bool clgl_sharing_ext;
            };

            class Platform
            {
                public:
                    Platform();
                    ~Platform();
                    void loadInfo(cl_platform_id plat_id);
                    void displayInfo(int i);

                    cl_platform_id platform_id;
                    std::string name;
                    std::string vendor;
                    std::string version;
                    std::string profile;
                    std::vector<Device> device_list;
            };

            enum class Vendor
            {
                AMD,
                NVIDIA,
                INTEL
            };

            Vendor mapPlatformToVendor(std::string str);            /**< Helper function that maps arbitrary vendor names to a well defined Enum. */
            void setupDevices(cl_context_properties* properties);   /**< Load the device currently assosciated with OpenGL. */
            void setupPlatforms();                                  /**< Display a list of OpenCL platforms and devices and select a platform. */

            static std::function<void(const std::string&, const std::string&, const std::string&)> setMessageCb;   /**< The function pointer to the RendererGUI message callback function. */

            Platform target_platform;               /**< The OpenCL platform ID fo the selected platform. */
            Device target_device;                   /**< The OpenCL device ID of the selected device. */
            cl_context context;                     /**< The OpenCL context. */
            cl_program rk_program;                  /**< The OpenCL program object containing the Rendering kernel data. */
            cl_program ppk_program;                 /**< The OpenCL program object containing the Post-processing kernel data. */
            cl_command_queue comm_queue;            /**< The OpenCL command queue.*/
            cl_kernel rend_kernel;                  /**< The main path-tracer kernel.*/
            cl_kernel pp_kernel;                    /**< The kernel for post processing effects like Tone mapping and Gamma Correction.*/
            cl_mem image_buffers[4];                /**< Image Buffer Objects. There are 2 for swapping role between read and write-only images. Third is for postprocessing*/
            cl_mem vert_buffer;                     /**< The Buffer Object used to hold Scene model data. */
            cl_mem mat_buffer;                      /**< The Buffer Object used to hold material data. */
            cl_mem bvh_buffer;                      /**< The Buffer Object used to hold bvh data. */
            cl_mem binary_heap_buffer;              /**< The Buffer Object used to hold binary heap which is used to traverse bvh. */
            cl_mem camera_buffer;                   /**< The Buffer Object used to hold Camera data. */

            size_t rendk_wgs;                       /**< The maximum nubmer of Work Items in a Workgroup the rendering kernel can afford due to memory limitations. */
            size_t ppk_wgs;                         /**< The maximum nubmer of Work Items in a Workgroup the post processing kernel can afford due to memory limitations. */
            size_t preferred_workgroup_multiple;    /**< The preferred multiple the of the local workgroup size (depends on the device). */


            friend class RendererCore;
    };
}
#endif // CLMANAGER_H
