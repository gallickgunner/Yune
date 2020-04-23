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

#include "CLManager.h"

#include <exception>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <limits>
#include <cctype>
#include <algorithm>

#ifdef _MSC_VER // Windows + VisualStudio
    #define NOMINMAX // Prevent <windows.h> from defining min/max macros.
    #include <Windows.h>
    #include <wingdi.h> // wglGetCurrentContext()
#endif

namespace yune
{
    std::function<void(const std::string&, const std::string&, const std::string&)> CLManager::setMessageCb;

    CLManager::Platform::Platform()
    {
    }

    CLManager::Platform::~Platform()
    {
    }

    CLManager::Device::Device() : max_workitem_size(20)
    {
    }

    CLManager::Device::~Device()
    {
    }

    CLManager::CLManager()
    {
        rend_kernel = NULL;
        pp_kernel = NULL;
        comm_queue = NULL;
        image_buffers[0] = NULL;
        image_buffers[1] = NULL;
        image_buffers[2] = NULL;
        vert_buffer = NULL;
        mat_buffer = NULL;
        bvh_buffer = NULL;
        camera_buffer = NULL;
        rk_program = NULL;
        ppk_program = NULL;
        context = NULL;
        //ctor
    }

    CLManager::~CLManager()
    {
        if(rend_kernel)
            clReleaseKernel(rend_kernel);
        if(pp_kernel)
            clReleaseKernel(pp_kernel);
        if(rk_program)
            clReleaseProgram(rk_program);
        if(ppk_program)
            clReleaseProgram(ppk_program);
        if(comm_queue)
            clReleaseCommandQueue(comm_queue);
        if(image_buffers[0])
            clReleaseMemObject(image_buffers[0]);
        if(image_buffers[1])
            clReleaseMemObject(image_buffers[1]);
        if(image_buffers[2])
            clReleaseMemObject(image_buffers[2]);
        if(vert_buffer)
            clReleaseMemObject(vert_buffer);
        if(mat_buffer)
            clReleaseMemObject(mat_buffer);
        if(bvh_buffer)
            clReleaseMemObject(bvh_buffer);
        if(camera_buffer)
            clReleaseMemObject(camera_buffer);
        if(context)
            clReleaseContext(context);
    }

    void CLManager::setGuiMessageCb(std::function<void(const std::string&, const std::string&, const std::string&)> cb)
    {
        CLManager::setMessageCb = cb;
    }

    void CLManager::setup()
    {
        int err;

        setupPlatforms();

        #if defined _WIN32
        static cl_context_properties properties[] =
        {
            CL_GL_CONTEXT_KHR, (cl_context_properties) wglGetCurrentContext(),
            CL_WGL_HDC_KHR, (cl_context_properties) wglGetCurrentDC(),
            CL_CONTEXT_PLATFORM, (cl_context_properties) target_platform.platform_id,
            0
        };
        #elif defined __linux
        static cl_context_properties properties[] =
        {
            CL_GL_CONTEXT_KHR, (cl_context_properties) glXGetCurrentContext(),
            CL_GLX_DISPLAY_KHR, (cl_context_properties) glXGetCurrentDisplay(),
            CL_CONTEXT_PLATFORM, (cl_context_properties) target_platform.platform_id,
            0
        };
        #elif defined(__APPLE__) || defined(__MACOSX)
        CGLContextObj glContext = CGLGetCurrentContext();
        CGLShareGroupObj shareGroup = CGLGetShareGroup(glContext);
        static cl_context_properties properties[] =
        {
            CL_CONTEXT_PROPERTY_USE_CGL_SHAREGROUP_APPLE,(cl_context_properties)shareGroup,
            0
        };
        #endif // _WIN_32

        setupDevices(properties);
        context = clCreateContext(properties, 1, &target_device.device_id, NULL, NULL, &err);
        checkError(err, __FILE__, __LINE__ - 1);

        comm_queue = clCreateCommandQueue(context, target_device.device_id, CL_QUEUE_PROFILING_ENABLE, &err);
        checkError(err, __FILE__, __LINE__ - 1);
    }

    bool CLManager::createRenderProgram(std::string fn, std::string path, bool reload)
    {
        try
        {
            std::cout << "\nReading Rendering Kernel File..." << std::endl;
            cl_int err=0;
            std::string rk;
            std::ifstream file;
            std::streamoff len;

            if(reload)
                path = rk_file_path;

            file.open(path, std::ios::binary);
            if(!file.is_open())
                throw std::runtime_error("Error opening file.");

            //Read rendering kernel files. Let RAII handle closing of file stream.
            file.seekg(0, std::ios::end);
            len = file.tellg();
            file.seekg(0, std::ios::beg);
            rk.resize(len);
            file.read(&rk[0], len);

            int first_char = rk.find_first_not_of(" \t\r\n");
            while(true)
            {
                const std::string::iterator last = std::find(rk.begin()+ first_char, rk.end(), '\n');
                std::string header(rk.begin() + first_char, last);
                if(header != "" && header.back() == '\r')
                    header.pop_back();
                std::stringstream ss(header);
                std::string word;
                ss >> word;
                if(word != "#yune-preproc")
                    break;
                else
                {
                    ss >> word;
                    if(word == "compiler-opts")
                        ss >> rk_compiler_opts;
                    else if (word == "kernel-name")
                        ss >> rk_name;
                }
                rk.erase(rk.begin(), last);
                first_char = 0;
            }
            std::cout << "File read successfully!" << std::endl;

            std::cout << "Compiling Kernel..," << std::endl;
            const char* rk_src = rk.c_str();
            rk_program = clCreateProgramWithSource(context, 1, &rk_src, NULL, &err);

            //Build Rendering Program
            if(rk_compiler_opts.empty())
                err = clBuildProgram(rk_program, 1, &target_device.device_id, NULL, NULL, NULL);
            else
                err = clBuildProgram(rk_program, 1, &target_device.device_id, rk_compiler_opts.data(), NULL, NULL);

            if(err < 0)
            {
                std::cout << "\nRendering Program failed to build." << std::endl;

                size_t log_size;
                err = clGetProgramBuildInfo(rk_program, target_device.device_id, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
                checkError(err, __FILE__, __LINE__ - 1);

                std::string log;
                log.resize(log_size);
                err = clGetProgramBuildInfo(rk_program, target_device.device_id, CL_PROGRAM_BUILD_LOG, log_size, &log[0], NULL);
                checkError(err, __FILE__, __LINE__ - 1);

                std::cout << "\n" + std::string(log) << std::endl;
                setMessageCb("Error in Kernel file. Log given below.", "Error!", std::string(log));
                return false;
            }

            rend_kernel = clCreateKernel(rk_program, rk_name.data(), &err);
            checkError(err, __FILE__, __LINE__ - 1);

            // Check memory and workgroup requirements for kernels. Check if Kernel's requirements exceed device capabilities.
            cl_ulong local_mem_size;
            size_t wgs, preferred_workgroup_multiple;

            clGetKernelWorkGroupInfo(rend_kernel, target_device.device_id, CL_KERNEL_WORK_GROUP_SIZE, sizeof(size_t), &wgs, NULL);
            clGetKernelWorkGroupInfo(rend_kernel, target_device.device_id, CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE, sizeof(size_t), &preferred_workgroup_multiple, NULL);
            clGetKernelWorkGroupInfo(rend_kernel, target_device.device_id, CL_KERNEL_LOCAL_MEM_SIZE, sizeof(cl_ulong), &local_mem_size, NULL);

            std::cout << "Kernel compiled successfully!" << std::endl;
            std::cout << "\nThe Rendering Kernel has the following features.." << std::endl;
            std::cout << "Kernel Workgroup Size: " << wgs << "\n"
                      << "Preferred WorkGroup Multiple Size: " <<  preferred_workgroup_multiple << "\n"
                      << "Kernel Local Memory Required: " << local_mem_size/(1024) << " KB" << "\n" << std::endl;

            if(local_mem_size > target_device.local_mem_size)
                throw std::runtime_error("Kernel local memory requirement exceeds Device's local memory.\nProgram may crash during kernel processing.\n");

            if(!reload)
            {
                rk_file = fn;
                rk_file_path = path;
            }
        }
        catch(const std::exception& err)
        {
            setMessageCb(err.what(), "Error!", "");
            return false;
        }
        setMessageCb("Rendering Kernel loaded successfully!", "Success!", "");
        return true;
    }

    bool CLManager::createPostProcProgram(std::string fn, std::string path, bool reload)
    {
        try
        {
            std::cout << "\nReading Post-processing Kernel File..." << std::endl;
            cl_int err=0;
            std::string pk;
            std::ifstream file;
            std::streamoff len;

            if(reload)
                path = ppk_file_path;

            file.open(path, std::ios::binary);
            if(!file.is_open())
                throw std::runtime_error("Error opening file.");

            //Read post processing file. Let RAII handle closing of file stream.
            file.seekg(0, std::ios::end);
            len = file.tellg();
            file.seekg(0, std::ios::beg);
            pk.resize(len);
            file.read(&pk[0], len);

            int first_char = pk.find_first_not_of(" \t\r\n");
            while(true)
            {
                const std::string::iterator last = std::find(pk.begin()+ first_char, pk.end(), '\n');
                std::string header(pk.begin() + first_char, last);
                if(header != "" && header.back() == '\r')
                    header.pop_back();
                std::stringstream ss(header);
                std::string word;
                ss >> word;
                if(word != "#yune-preproc")
                    break;
                else
                {
                    ss >> word;
                    if(word == "compiler-opts")
                        ss >> ppk_compiler_opts;
                    else if (word == "kernel-name")
                        ss >> ppk_name;
                }
                pk.erase(pk.begin(), last);
                first_char = 0;
            }
            std::cout << "File read successfully!" << std::endl;

            std::cout << "Compiling Kernel..." << std::endl;
            const char* pk_src = pk.c_str();
            ppk_program = clCreateProgramWithSource(context, 1, &pk_src, NULL, &err);

            //Build Post-processing Program
            if(ppk_compiler_opts.empty())
                err = clBuildProgram(ppk_program, 1, &target_device.device_id, NULL, NULL, NULL);
            else
                err = clBuildProgram(ppk_program, 1, &target_device.device_id, ppk_compiler_opts.data(), NULL, NULL);
            if(err < 0)
            {
                std::cout << "\nPost-processing Program failed to build." << std::endl;

                size_t log_size;
                err = clGetProgramBuildInfo(ppk_program, target_device.device_id, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
                checkError(err, __FILE__, __LINE__ - 1);

                std::string log;
                log.resize(log_size);
                err = clGetProgramBuildInfo(ppk_program, target_device.device_id, CL_PROGRAM_BUILD_LOG, log_size, &log[0], NULL);
                checkError(err, __FILE__, __LINE__ - 1);
                std::cout << "\n" + std::string(log) << std::endl;

                setMessageCb("Error in Kernel file. Log given below.", "Error!", std::string(log));
                return false;
            }

            pp_kernel = clCreateKernel(ppk_program, ppk_name.data(), &err);
            checkError(err, __FILE__, __LINE__ - 1);

            // Check memory and workgroup requirements for kernels. Check if Kernel's requirements exceed device capabilities.
            cl_ulong local_mem_size;
            size_t wgs, preferred_workgroup_multiple;

            clGetKernelWorkGroupInfo(pp_kernel, target_device.device_id, CL_KERNEL_WORK_GROUP_SIZE, sizeof(size_t), &wgs, NULL);
            clGetKernelWorkGroupInfo(pp_kernel, target_device.device_id, CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE, sizeof(size_t), &preferred_workgroup_multiple, NULL);
            clGetKernelWorkGroupInfo(pp_kernel, target_device.device_id, CL_KERNEL_LOCAL_MEM_SIZE, sizeof(cl_ulong), &local_mem_size, NULL);

            std::cout << "Kernel compiled successfully!" << std::endl;
            std::cout << "\nThe Post-Processing Kernel has the following features.." << std::endl;
            std::cout << "Kernel Workgroup Size: " << wgs << "\n"
                      << "Preferred WorkGroup Multiple Size: " <<  preferred_workgroup_multiple << "\n"
                      << "Kernel Local Memory Required: " << local_mem_size/(1024) << " KB" << "\n" << std::endl;

            if(local_mem_size > target_device.local_mem_size)
                throw std::runtime_error("Kernel local memory requirement exceeds Device's local memory.\nProgram may crash during kernel processing.\n");

            if(!reload)
            {
                ppk_file = fn;
                ppk_file_path = path;
            }
        }
        catch(const std::exception& err)
        {
            setMessageCb(err.what(), "Error!", "");
            return false;
        }
        setMessageCb("Post-processing Kernel loaded successfully!", "Success!", "");
        return true;
    }

    bool CLManager::setupImageBuffers(GLuint* rbo_IDs)
    {
        try
        {
            cl_int err = 0;

            //If buffers already allocated, this means window size changed and need to allocate new buffer so delete previous
            if(image_buffers[0])
                clReleaseMemObject(image_buffers[0]);
            if(image_buffers[1])
                clReleaseMemObject(image_buffers[1]);
            if(image_buffers[2])
                clReleaseMemObject(image_buffers[2]);

            //Setup Image Buffers for reading/writing and Post processing.
            image_buffers[0] = clCreateFromGLRenderbuffer(context, CL_MEM_READ_WRITE, rbo_IDs[0], &err);
            checkError(err, __FILE__, __LINE__ - 1);

            image_buffers[1] = clCreateFromGLRenderbuffer(context, CL_MEM_READ_WRITE, rbo_IDs[1], &err);
            checkError(err, __FILE__, __LINE__ - 1);

            image_buffers[2] = clCreateFromGLRenderbuffer(context, CL_MEM_READ_WRITE, rbo_IDs[2], &err);
            checkError(err, __FILE__, __LINE__ - 1);
        }
        catch(const std::exception& err)
        {
            setMessageCb(err.what(), "Error Creating Image Buffer", "");
            return false;
        }
        return true;
    }

    void CLManager::setupCameraBuffer(Cam* cam_data)
    {
        cl_int err = 0;
        camera_buffer = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_ALLOC_HOST_PTR | CL_MEM_COPY_HOST_PTR, sizeof(Cam), cam_data, &err);
        checkError(err, __FILE__, __LINE__ - 1);
    }

    bool CLManager::setupBVHBuffer(std::vector<BVHNodeGPU>& bvh_data, float bvh_size, float scene_size)
    {
        try
        {
            cl_int err = 0;
            if(bvh_buffer)
                clReleaseMemObject(bvh_buffer);

            if(bvh_size + scene_size > target_device.global_mem_size)
                throw std::runtime_error("BVH and Scene Data size combined exceed Device's global memory size.");

            bvh_buffer = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_ALLOC_HOST_PTR | CL_MEM_COPY_HOST_PTR, sizeof(BVHNodeGPU) * bvh_data.size(), bvh_data.data(), &err);
            checkError(err, __FILE__, __LINE__);
        }
        catch(const std::exception& err)
        {
            setMessageCb(err.what(), "Error Creating BVH Buffer", "");
            return false;
        }
        return true;
    }

    bool CLManager::setupVertexBuffer(std::vector<TriangleGPU>& vert_data, float scene_size)
    {
        try
        {
            cl_int err = 0;

            if(vert_buffer)
                clReleaseMemObject(vert_buffer);

            //Check if Material and Vertex Data combined exceed Device's global memory.
            if( scene_size > target_device.global_mem_size)
                throw std::runtime_error("Scene Data size exceeds Device's global memory size.");

            vert_buffer = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_ALLOC_HOST_PTR | CL_MEM_COPY_HOST_PTR, sizeof(TriangleGPU) * vert_data.size(), vert_data.data(), &err);
            checkError(err, __FILE__, __LINE__);
        }
        catch(const std::exception& err)
        {
            setMessageCb(err.what(), "Error Creating Vertex Buffer", "");
            return false;
        }
        return true;
    }

    bool CLManager::setupMatBuffer(std::vector<Material>& mat_data)
    {
        try
        {
            cl_int err = 0;

            if(mat_buffer)
                clReleaseMemObject(mat_buffer);

            mat_buffer = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_ALLOC_HOST_PTR | CL_MEM_COPY_HOST_PTR, sizeof(Material) * mat_data.size(), mat_data.data(), &err);
            checkError(err, __FILE__, __LINE__);
        }
        catch(const std::exception& err)
        {
            setMessageCb(err.what(), "Error Creating Material Buffer", "");
            return false;
        }
        return true;
    }

    void CLManager::setupPlatforms()
    {
        int plat_choice;
        cl_uint num_plat =0, num_dev=0, err=0;
        clGetPlatformIDs(0, NULL, &num_plat);
        if(num_plat == 0)
            throw std::runtime_error("Failed to detect any platform.");

        //Display platform information on which OpenGL context was created.
        /*std::string renderer = (const char*) (glGetString(GL_RENDERER));
        std::string vendor = (const char*) (glGetString(GL_VENDOR));

        std::cout << "Platform Information on which OpenGL context was created.\n\n"
                  << std::left << std::setw(10) << "VENDOR" << ": " << vendor << "\n"
                  << std::left << std::setw(10) << "RENDERER" << ": " << renderer << "\n" << std::endl;*/

        std::string vendor = "AMD";
        CLManager::Vendor vd = mapPlatformToVendor(vendor);

        // List of OpenCL platforms and Devices
        std::vector<cl_platform_id> platforms(num_plat);
        std::vector<Platform> platform_list;

        err = clGetPlatformIDs(num_plat, platforms.data(), NULL);
        checkError(err, __FILE__, __LINE__ - 1);

        std::cout << "Platform Information on which OpenCL context can be created." << std::endl;
        std::cout << platforms.size() << "OpenCL platform(s) detected..\n" << std::endl;
        bool clgl_interop = false;
        int plat_idx = -1;

        for(int i = 0; i < platforms.size(); i++)
        {
            platform_list.push_back(Platform());
            platform_list[i].loadInfo(platforms[i]);
            platform_list[i].displayInfo(i);

            //Query for Device information.
            err = clGetDeviceIDs(platforms[i], CL_DEVICE_TYPE_ALL, 0, NULL, &num_dev);
            checkError(err, __FILE__, __LINE__ - 1);

            if(num_dev == 0)
            {
                std::cout << "No Devices detected on this platform..." << std::endl;
                continue;
            }

            std::vector<cl_device_id> devices(num_dev);
            err = clGetDeviceIDs(platforms[i], CL_DEVICE_TYPE_ALL, num_dev, devices.data(), NULL);
            checkError(err, __FILE__, __LINE__ - 1);

            for(int j = 0; j < devices.size(); j++)
            {
                platform_list[i].device_list.push_back(Device());
                platform_list[i].device_list[j].loadInfo(devices[j]);
                platform_list[i].device_list[j].displayInfo(j);
                if(platform_list[i].device_list[j].clgl_sharing_ext)
                    clgl_interop = true;
            }
            if(!clgl_interop)
                std::cout << "No devices present on this platform that support OpenCL-GL interoperability.\n" << std::endl;

            //Set platform same as the platform OpenGL was created on.
            CLManager::Vendor temp = mapPlatformToVendor(platform_list[i].vendor);
            if (temp == vd)
            {
                target_platform = platform_list[i];
                plat_idx = i;
            }
        }

        std::cout << "\nSelected Platform " << plat_idx << std::endl;
    }

    void CLManager::setupDevices(cl_context_properties* properties)
    {
        int dev_choice = 0; // default to first entry
        size_t num_devices;
        cl_int err;
        cl_device_id device_id;

        // Load extension
        clGetGLContextInfoKHR_fn clGetGLContextInfoKHR = NULL;

        #if CL_TARGET_OPENCL_VERSION > 110
        clGetGLContextInfoKHR = (clGetGLContextInfoKHR_fn) clGetExtensionFunctionAddressForPlatform(target_platform.platform_id, "clGetGLContextInfoKHR");
        #else
        clGetGLContextInfoKHR = (clGetGLContextInfoKHR_fn) clGetExtensionFunctionAddress("clGetGLContextInfoKHR");
        #endif
        if(!clGetGLContextInfoKHR)
            throw std::runtime_error("\"clGetGLContextInfoKHR\" Function failed to load.");

        err = clGetGLContextInfoKHR(properties, CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR, sizeof(cl_device_id), &device_id, &num_devices);
        checkError(err, __FILE__, __LINE__ - 1);

        num_devices = num_devices/sizeof(cl_device_id);
        if(num_devices == 0)
        {
            std::string error = "\nEither there is no device available or they don't support OpenCL-GL interoperability\"";
            throw std::runtime_error(error);
        }

        for(int i = 0; i < target_platform.device_list.size(); i++)
        {
            if(device_id == target_platform.device_list[i].device_id)
            {
                target_device = target_platform.device_list[i];
                std::cout << "Selected Device " << i << std::endl;
            }
        }
        std::cout << "\nPress Enter to continue.." << std::endl;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }

    CLManager::Vendor CLManager::mapPlatformToVendor(std::string str)
    {
        CLManager::Vendor vd = Vendor::UNKNOWN;
        std::transform(str.begin(), str.end(), str.begin(), ( int(&)(int) )std::tolower);

        if(str.find("ati") != std::string::npos )
            vd = Vendor::AMD;
        else if(str.find("amd") != std::string::npos )
            vd = Vendor::AMD;
        else if(str.find("advanced micro devices") != std::string::npos )
            vd = Vendor::AMD;
        else if(str.find("nvidia") != std::string::npos )
            vd = Vendor::NVIDIA;
        else if(str.find("intel") != std::string::npos )
            vd = Vendor::INTEL;
        return vd;
    }

    void CLManager::Platform::loadInfo(cl_platform_id plat_id)
    {
        platform_id = plat_id;
        {
            size_t len = 0;
            clGetPlatformInfo(platform_id, CL_PLATFORM_NAME, 0, NULL, &len);
            name.resize(len);
            clGetPlatformInfo(platform_id, CL_PLATFORM_NAME, len, &name[0], NULL);
        }

        {
            size_t len = 0;
            clGetPlatformInfo(platform_id, CL_PLATFORM_VENDOR, 0, NULL, &len);
            vendor.resize(len);
            clGetPlatformInfo(platform_id, CL_PLATFORM_VENDOR, len, &vendor[0], NULL);
        }

        {
            size_t len = 0;
            clGetPlatformInfo(platform_id, CL_PLATFORM_VERSION, 0, NULL, &len);
            version.resize(len);
            clGetPlatformInfo(platform_id, CL_PLATFORM_VERSION, len, &version[0], NULL);
        }

        {
            size_t len = 0;
            clGetPlatformInfo(platform_id, CL_PLATFORM_PROFILE, 0 , NULL, &len);
            profile.resize(len);
            clGetPlatformInfo(platform_id, CL_PLATFORM_PROFILE, len, &profile[0], NULL);
        }
    }

    void CLManager::Platform::displayInfo(int i)
    {
        std::cout << "Platform " << i << "\n"
                  << "-------------------------------------------------------------------------------\n"
                  << std::left << std::setw(35) << "Platform Name" << ": " << name << "\n"
                  << std::left << std::setw(35) << "Vendor Name" << ": " << vendor << "\n"
                  << std::left << std::setw(35) << "OpenCL Profile" << ": " << profile << "\n"
                  << std::left << std::setw(35) << "OpenCL Version Supported" << ": " << version << "\n"
                  << "--------------------------------------------------------------------------------\n"
                  << std::endl;
    }

    void CLManager::Device::loadInfo(cl_device_id dev_id)
    {
        device_id = dev_id;
        size_t len;
        {
            clGetDeviceInfo(device_id, CL_DEVICE_NAME, 0, NULL, &len);
            name.resize(len);
            clGetDeviceInfo(device_id, CL_DEVICE_NAME, len, &name[0], NULL);
        }

        {
            clGetDeviceInfo(device_id, CL_DEVICE_VERSION, 0, NULL, &len);
            device_ver.resize(len);
            clGetDeviceInfo(device_id, CL_DEVICE_VERSION, len, &device_ver[0], NULL);
            
        }

        {
            clGetDeviceInfo(device_id, CL_DRIVER_VERSION, 0, NULL, &len);
            driver_ver.resize(len);
            clGetDeviceInfo(device_id, CL_DRIVER_VERSION, len, &driver_ver[0], NULL);
            
        }

        {
            clGetDeviceInfo(device_id, CL_DEVICE_OPENCL_C_VERSION, 0, NULL, &len);
            device_openclC_ver.resize(len);
            clGetDeviceInfo(device_id, CL_DEVICE_OPENCL_C_VERSION, len, &device_openclC_ver[0], NULL);
            
        }

        {
            clGetDeviceInfo(device_id, CL_DEVICE_EXTENSIONS, 0 , NULL, &len);
            ext.resize(len);
            clGetDeviceInfo(device_id, CL_DEVICE_EXTENSIONS, len, &ext[0], NULL);
        }

        cl_uint dims;
        clGetDeviceInfo(device_id, CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS, sizeof(cl_uint), &dims, NULL);
        clGetDeviceInfo(device_id, CL_DEVICE_TYPE, sizeof(cl_device_type), &device_type, NULL);
        clGetDeviceInfo(device_id, CL_DEVICE_AVAILABLE, sizeof(cl_bool), &availability, NULL);
        clGetDeviceInfo(device_id, CL_DEVICE_SINGLE_FP_CONFIG, sizeof(cl_device_fp_config), &fp_support, NULL);
        clGetDeviceInfo(device_id, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(cl_uint), &compute_units, NULL);
        clGetDeviceInfo(device_id, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(size_t), &wg_size, NULL);
        clGetDeviceInfo(device_id, CL_DEVICE_MAX_WORK_ITEM_SIZES, sizeof(size_t)*dims, max_workitem_size.data(), NULL);
        clGetDeviceInfo(device_id, CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(cl_ulong), &global_mem_size, NULL);
        clGetDeviceInfo(device_id, CL_DEVICE_LOCAL_MEM_SIZE, sizeof(cl_ulong), &local_mem_size, NULL);
        clGetDeviceInfo(device_id, CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE, sizeof(cl_ulong), &constant_mem_size, NULL);
    }

    void CLManager::Device::displayInfo(int i)
    {
        if(ext.find(CL_GL_SHARING_EXT) == std::string::npos)
        {
            clgl_sharing_ext = false;
            return;
        }
        clgl_sharing_ext = true;

        std::string type;
        if(device_type & CL_DEVICE_TYPE_CPU)
            type = "CPU";
        else if(device_type & CL_DEVICE_TYPE_GPU)
            type = "GPU";
        else if(device_type & CL_DEVICE_TYPE_ACCELERATOR)
            type = "ACCELERATOR";

        if(ext.find("cl_khr_gl_event") != std::string::npos)
            clgl_event_ext = true;
        else
            clgl_event_ext = false;

        std::cout << "Device " << i << "\n"
                  << "-------------------------------------------------------------------------------\n"
                  << std::left << std::setw(35) << "Device Name" << ": " << name << "\n"
                  << std::left << std::setw(35) << "Device Type" << ": " << type << "\n"
                  << std::left << std::setw(35) << "OpenCL Version Supported" <<  ": " << device_ver << "\n"
                  << std::left << std::setw(35) << "OpenCL Software Driver Version" << ": " << driver_ver << "\n"
                  << std::left << std::setw(35) << "OpenCL-C Version Supported" << ": " <<  device_openclC_ver << "\n"
                  << std::left << std::setw(35) << "Device Available" << ": " << (availability ? "Yes" : "No") << "\n"
                  << std::left << std::setw(35) << "cl_khr_gl_event Supported" << ": " << (clgl_event_ext ? "Yes" : "No") << "\n"
                  << std::left << std::setw(35) << std::string(CL_GL_SHARING_EXT) + " Supported" << ": " << (clgl_sharing_ext ? "Yes" : "No") << "\n"
                  << std::left << std::setw(35) << "SP Floating Point Supported" << ": " << ( (fp_support & (CL_FP_ROUND_TO_NEAREST|CL_FP_INF_NAN) ) ? "Yes" : "No") << "\n"
                  << std::left << std::setw(35) << "Max Compute Units" << ": " << compute_units << "\n"
                  << std::left << std::setw(35) << "Max Workgroup Size" << ": " << wg_size << "\n"
                  << std::left << std::setw(35) << "Max WorkItem Size" << ": " << max_workitem_size[0] << ", " << max_workitem_size[1] << "\n"
                  << std::left << std::setw(35) << "Max Global Memory Size" << ": " << global_mem_size/(1024*1024) << " MB" << "\n"
                  << std::left << std::setw(35) << "Max Local Memory Size" << ": " << local_mem_size/(1024) << " KB" << "\n"
                  << std::left << std::setw(35) << "Max Constant Memory Size" << ": " << constant_mem_size/ (1024) << " KB" << "\n"
                  << "--------------------------------------------------------------------------------"
                  << std::endl;
    }

    void CLManager::checkError(cl_int err_code, std::string filename, int line_number)
    {

        std::string line = std::to_string(line_number);
        std::string msg = " in File: \"" + filename + "\" at Line number: " + line;
        if(err_code >= 0)
            return;

        switch (err_code)
        {
            case -1: throw std::runtime_error("CL_DEVICE_NOT_FOUND" + msg);
            case -2: throw std::runtime_error("CL_DEVICE_NOT_AVAILABLE" + msg);
            case -3: throw std::runtime_error("CL_COMPILER_NOT_AVAILABLE" + msg);
            case -4: throw std::runtime_error("CL_MEM_OBJECT_ALLOCATION_FAILURE" + msg);
            case -5: throw std::runtime_error("CL_OUT_OF_RESOURCES" + msg);
            case -6: throw std::runtime_error("CL_OUT_OF_HOST_MEMORY" + msg);
            case -7: throw std::runtime_error("CL_PROFILING_INFO_NOT_AVAILABLE" + msg);
            case -8: throw std::runtime_error("CL_MEM_COPY_OVERLAP" + msg);
            case -9: throw std::runtime_error("CL_IMAGE_FORMAT_MISMATCH" + msg);
            case -10: throw std::runtime_error("CL_IMAGE_FORMAT_NOT_SUPPORTED" + msg);
            case -12: throw std::runtime_error("CL_MAP_FAILURE" + msg);
            case -13: throw std::runtime_error("CL_MISALIGNED_SUB_BUFFER_OFFSET" + msg);
            case -14: throw std::runtime_error("CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST" + msg);
            case -15: throw std::runtime_error("CL_COMPILE_PROGRAM_FAILURE" + msg);
            case -16: throw std::runtime_error("CL_LINKER_NOT_AVAILABLE" + msg);
            case -17: throw std::runtime_error("CL_LINK_PROGRAM_FAILURE" + msg);
            case -18: throw std::runtime_error("CL_DEVICE_PARTITION_FAILED" + msg);
            case -19: throw std::runtime_error("CL_KERNEL_ARG_INFO_NOT_AVAILABLE" + msg);
            case -30: throw std::runtime_error("CL_INVALID_VALUE" + msg);
            case -31: throw std::runtime_error("CL_INVALID_DEVICE_TYPE" + msg);
            case -32: throw std::runtime_error("CL_INVALID_PLATFORM" + msg);
            case -33: throw std::runtime_error("CL_INVALID_DEVICE" + msg);
            case -34: throw std::runtime_error("CL_INVALID_CONTEXT" + msg);
            case -35: throw std::runtime_error("CL_INVALID_QUEUE_PROPERTIES" + msg);
            case -36: throw std::runtime_error("CL_INVALID_COMMAND_QUEUE" + msg);
            case -37: throw std::runtime_error("CL_INVALID_HOST_PTR" + msg);
            case -38: throw std::runtime_error("CL_INVALID_MEM_OBJECT" + msg);
            case -39: throw std::runtime_error("CL_INVALID_IMAGE_FORMAT_DESCRIPTOR" + msg);
            case -40: throw std::runtime_error("CL_INVALID_IMAGE_SIZE" + msg);
            case -41: throw std::runtime_error("CL_INVALID_SAMPLER" + msg);
            case -42: throw std::runtime_error("CL_INVALID_BINARY" + msg);
            case -43: throw std::runtime_error("CL_INVALID_BUILD_OPTIONS" + msg);
            case -44: throw std::runtime_error("CL_INVALID_PROGRAM" + msg);
            case -45: throw std::runtime_error("CL_INVALID_PROGRAM_EXECUTABLE" + msg);
            case -46: throw std::runtime_error("CL_INVALID_KERNEL_NAME" + msg);
            case -47: throw std::runtime_error("CL_INVALID_KERNEL_DEFINITION" + msg);
            case -48: throw std::runtime_error("CL_INVALID_KERNEL" + msg);
            case -49: throw std::runtime_error("CL_INVALID_ARG_INDEX" + msg);
            case -50: throw std::runtime_error("CL_INVALID_ARG_VALUE" + msg);
            case -51: throw std::runtime_error("CL_INVALID_ARG_SIZE" + msg);
            case -52: throw std::runtime_error("CL_INVALID_KERNEL_ARGS" + msg);
            case -53: throw std::runtime_error("CL_INVALID_WORK_DIMENSION" + msg);
            case -54: throw std::runtime_error("CL_INVALID_WORK_GROUP_SIZE" + msg);
            case -55: throw std::runtime_error("CL_INVALID_WORK_ITEM_SIZE" + msg);
            case -56: throw std::runtime_error("CL_INVALID_GLOBAL_OFFSET" + msg);
            case -57: throw std::runtime_error("CL_INVALID_EVENT_WAIT_LIST" + msg);
            case -58: throw std::runtime_error("CL_INVALID_EVENT" + msg);
            case -59: throw std::runtime_error("CL_INVALID_OPERATION" + msg);
            case -60: throw std::runtime_error("CL_INVALID_GL_OBJECT" + msg);
            case -61: throw std::runtime_error("CL_INVALID_BUFFER_SIZE" + msg);
            case -62: throw std::runtime_error("CL_INVALID_MIP_LEVEL" + msg);
            case -63: throw std::runtime_error("CL_INVALID_GLOBAL_WORK_SIZE" + msg);
            case -64: throw std::runtime_error("CL_INVALID_PROPERTY" + msg);
            case -65: throw std::runtime_error("CL_INVALID_IMAGE_DESCRIPTOR" + msg);
            case -66: throw std::runtime_error("CL_INVALID_COMPILER_OPTIONS" + msg);
            case -67: throw std::runtime_error("CL_INVALID_LINKER_OPTIONS" + msg);
            case -68: throw std::runtime_error("CL_INVALID_DEVICE_PARTITION_COUNT" + msg);
            case -69: throw std::runtime_error("CL_INVALID_PIPE_SIZE" + msg);
            case -70: throw std::runtime_error("CL_INVALID_DEVICE_QUEUE" + msg);
            case -71: throw std::runtime_error("CL_INVALID_SPEC_ID" + msg);
            case -72: throw std::runtime_error("CL_MAX_SIZE_RESTRICTION_EXCEEDED" + msg);
            case -1002: throw std::runtime_error("CL_INVALID_D3D10_DEVICE_KHR" + msg);
            case -1003: throw std::runtime_error("CL_INVALID_D3D10_RESOURCE_KHR" + msg);
            case -1004: throw std::runtime_error("CL_D3D10_RESOURCE_ALREADY_ACQUIRED_KHR" + msg);
            case -1005: throw std::runtime_error("CL_D3D10_RESOURCE_NOT_ACQUIRED_KHR" + msg);
            case -1006: throw std::runtime_error("CL_INVALID_D3D11_DEVICE_KHR" + msg);
            case -1007: throw std::runtime_error("CL_INVALID_D3D11_RESOURCE_KHR" + msg);
            case -1008: throw std::runtime_error("CL_D3D11_RESOURCE_ALREADY_ACQUIRED_KHR" + msg);
            case -1009: throw std::runtime_error("CL_D3D11_RESOURCE_NOT_ACQUIRED_KHR" + msg);
            case -1010: throw std::runtime_error("CL_INVALID_DX9_MEDIA_ADAPTER_KHR" + msg);
            case -1011: throw std::runtime_error("CL_INVALID_DX9_MEDIA_SURFACE_KHR" + msg);
            case -1012: throw std::runtime_error("CL_DX9_MEDIA_SURFACE_ALREADY_ACQUIRED_KHR" + msg);
            case -1013: throw std::runtime_error("CL_DX9_MEDIA_SURFACE_NOT_ACQUIRED_KHR" + msg);
            case -1093: throw std::runtime_error("CL_INVALID_EGL_OBJECT_KHR" + msg);
            case -1092: throw std::runtime_error("CL_EGL_RESOURCE_NOT_ACQUIRED_KHR" + msg);
            case -1001: throw std::runtime_error("CL_PLATFORM_NOT_FOUND_KHR" + msg);
            case -1057: throw std::runtime_error("CL_DEVICE_PARTITION_FAILED_EXT" + msg);
            case -1058: throw std::runtime_error("CL_INVALID_PARTITION_COUNT_EXT" + msg);
            case -1059: throw std::runtime_error("CL_INVALID_PARTITION_NAME_EXT" + msg);
            case -1094: throw std::runtime_error("CL_INVALID_ACCELERATOR_INTEL" + msg);
            case -1095: throw std::runtime_error("CL_INVALID_ACCELERATOR_TYPE_INTEL" + msg);
            case -1096: throw std::runtime_error("CL_INVALID_ACCELERATOR_DESCRIPTOR_INTEL" + msg);
            case -1097: throw std::runtime_error("CL_ACCELERATOR_TYPE_NOT_SUPPORTED_INTEL" + msg);
            case -1000: throw std::runtime_error("CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR" + msg);
            case -1098: throw std::runtime_error("CL_INVALID_VA_API_MEDIA_ADAPTER_INTEL" + msg);
            case -1099: throw std::runtime_error("CL_INVALID_VA_API_MEDIA_SURFACE_INTEL" + msg);
            case -1100: throw std::runtime_error("CL_VA_API_MEDIA_SURFACE_ALREADY_ACQUIRED_INTEL" + msg);
            case -1101: throw std::runtime_error("CL_VA_API_MEDIA_SURFACE_NOT_ACQUIRED_INTEL" + msg);
            default: throw std::runtime_error("CL_UNKNOWN_ERROR" + msg);
        }
    }


}
