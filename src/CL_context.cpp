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

#include <RuntimeError.h>
#include <CL_context.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <string>
#include <limits>
#include <cctype>
#include <algorithm>

namespace yune
{
    CL_context::Platform::Platform()
    {
    }

    CL_context::Platform::~Platform()
    {
    }

    CL_context::Device::Device() : max_workitem_size(20)
    {
    }

    CL_context::Device::~Device()
    {
    }

    CL_context::CL_context()
    {
        rend_kernel = NULL;
        pp_kernel = NULL;
        comm_queue = NULL;
        image_buffers[0] = NULL;
        image_buffers[1] = NULL;
        image_buffers[2] = NULL;
        scene_buffer = NULL;
        mat_buffer = NULL;
        camera_buffer = NULL;
        rk_program = NULL;
        ppk_program = NULL;
        context = NULL;
        //ctor
    }

    CL_context::~CL_context()
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
        if(scene_buffer)
            clReleaseMemObject(scene_buffer);
        if(mat_buffer)
            clReleaseMemObject(mat_buffer);
        if(camera_buffer)
            clReleaseMemObject(camera_buffer);
        if(context)
            clReleaseContext(context);
    }

    void CL_context::setup()
    {
        try
        {
            int err, reset = 1;

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
        catch(const RuntimeError& err)
        {
            throw;
        }
    }

    void CL_context::createProgram(std::string opts, std::string pt_fn, std::string pt_kn, std::string pp_fn, std::string pp_kn)
    {
        cl_int err=0;

        std::string rk, pk;
        std::ifstream file, file2;
        std::streamoff len;
        file.open(pt_fn, std::ios::binary);
        file2.open(pp_fn, std::ios::binary);

        //Read rendering and post processing files. Let RAII handle closing of file stream.
        file.seekg(0, std::ios::end);
        len = file.tellg();
        file.seekg(0, std::ios::beg);
        rk.resize(len);
        file.read(&rk[0], len);

        file2.seekg(0, std::ios::end);
        len = file2.tellg();
        file2.seekg(0, std::ios::beg);
        pk.resize(len);
        file2.read(&pk[0], len);

        const char* str1 = rk.c_str();
        const char* str2 = pk.c_str();
        rk_program = clCreateProgramWithSource(context, 1, &str1, NULL, &err);
        ppk_program = clCreateProgramWithSource(context, 1, &str2, NULL, &err);

        //Build Rendering Program
        if(opts.empty())
            err = clBuildProgram(rk_program, 1, &target_device.device_id, NULL, NULL, NULL);
        else
            err = clBuildProgram(rk_program, 1, &target_device.device_id, opts.data(), NULL, NULL);
        if(err < 0)
        {
            std::cout << "\nRendering Program failed to build.." << std::endl;

            size_t log_size;
            err = clGetProgramBuildInfo(rk_program, target_device.device_id, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
            checkError(err, __FILE__, __LINE__ - 1);

            char log[log_size];
            err = clGetProgramBuildInfo(rk_program, target_device.device_id, CL_PROGRAM_BUILD_LOG, log_size, log, NULL);
            checkError(err, __FILE__, __LINE__ - 1);
            throw RuntimeError("\n" + std::string(log) + "\n");
        }

        //Build Post-processing Program
        if(opts.empty())
            err = clBuildProgram(ppk_program, 1, &target_device.device_id, NULL, NULL, NULL);
        else
            err = clBuildProgram(ppk_program, 1, &target_device.device_id, opts.data(), NULL, NULL);
        if(err < 0)
        {
            std::cout << "\nPost-processing Program failed to build.." << std::endl;

            size_t log_size;
            err = clGetProgramBuildInfo(ppk_program, target_device.device_id, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
            checkError(err, __FILE__, __LINE__ - 1);

            char log[log_size];
            err = clGetProgramBuildInfo(ppk_program, target_device.device_id, CL_PROGRAM_BUILD_LOG, log_size, log, NULL);
            checkError(err, __FILE__, __LINE__ - 1);
            throw RuntimeError("\n" + std::string(log) + "\n");
        }

        rend_kernel = clCreateKernel(rk_program, pt_kn.data(), &err);
        checkError(err, __FILE__, __LINE__ - 1);

        pp_kernel = clCreateKernel(ppk_program, pp_kn.data(), &err);
        checkError(err, __FILE__, __LINE__ - 1);

        // Check memory and workgroup requirements for kernels. Check if Kernel's requirements exceed device capabilities.
        cl_ulong local_mem_size;
        size_t wgs, preferred_workgroup_multiple;

        clGetKernelWorkGroupInfo(rend_kernel, target_device.device_id, CL_KERNEL_WORK_GROUP_SIZE, sizeof(size_t), &wgs, NULL);
        clGetKernelWorkGroupInfo(rend_kernel, target_device.device_id, CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE, sizeof(size_t), &preferred_workgroup_multiple, NULL);
        clGetKernelWorkGroupInfo(rend_kernel, target_device.device_id, CL_KERNEL_LOCAL_MEM_SIZE, sizeof(cl_ulong), &local_mem_size, NULL);

        std::cout << "\nThe Rendering Kernel has the following features.." << std::endl;
        std::cout << "Kernel Workgroup Size: " << wgs << "\n"
                  << "Preferred WorkGroup Multiple Size: " <<  preferred_workgroup_multiple << "\n"
                  << "Kernel Local Memory Required: " << local_mem_size/(1024) << " KB" << "\n" << std::endl;

        if(local_mem_size > target_device.local_mem_size)
            std::cout << "Kernel local memory requirement exceeds Device's local memory.\nProgram may crash during kernel processing.\n" << std::endl;

        clGetKernelWorkGroupInfo(rend_kernel, target_device.device_id, CL_KERNEL_WORK_GROUP_SIZE, sizeof(size_t), &wgs, NULL);
        clGetKernelWorkGroupInfo(rend_kernel, target_device.device_id, CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE, sizeof(size_t), &preferred_workgroup_multiple, NULL);
        clGetKernelWorkGroupInfo(rend_kernel, target_device.device_id, CL_KERNEL_LOCAL_MEM_SIZE, sizeof(cl_ulong), &local_mem_size, NULL);

        std::cout << "\nThe Post-Processing Kernel has the following features.." << std::endl;
        std::cout << "Kernel Workgroup Size: " << wgs << "\n"
                  << "Preferred WorkGroup Multiple Size: " <<  preferred_workgroup_multiple << "\n"
                  << "Kernel Local Memory Required: " << local_mem_size/(1024) << " KB" << "\n" << std::endl;

        if(local_mem_size > target_device.local_mem_size)
            std::cout << "Kernel local memory requirement exceeds Device's local memory.\nProgram may crash during kernel processing.\n" << std::endl;
    }

    void CL_context::setupBuffers(GLuint* rbo_IDs, std::vector<Triangle>& scene_data, std::vector<Material>& mat_data, Cam* cam_data)
    {
        cl_int err = 0;

        //Check if buffer size exceed Device's global memory.
        if( scene_data.size() > target_device.global_mem_size)
            throw RuntimeError("Model Data size exceeds Device's global memory size.\nProgram may crash during kernel processing.");

        //Setup Image Buffers for reading/writing and Post processing. Also setup Buffer Objects containing scene data and camera data.
        image_buffers[0] = clCreateFromGLRenderbuffer(context, CL_MEM_READ_WRITE, rbo_IDs[0], &err);
        checkError(err, __FILE__, __LINE__ - 1);

        image_buffers[1] = clCreateFromGLRenderbuffer(context, CL_MEM_READ_WRITE, rbo_IDs[1], &err);
        checkError(err, __FILE__, __LINE__ - 1);

        image_buffers[2] = clCreateFromGLRenderbuffer(context, CL_MEM_READ_WRITE, rbo_IDs[2], &err);
        checkError(err, __FILE__, __LINE__ - 1);

        camera_buffer = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_ALLOC_HOST_PTR | CL_MEM_COPY_HOST_PTR, sizeof(Cam), cam_data, &err);
        checkError(err, __FILE__, __LINE__ - 1);

        if(!scene_data.empty())
        {
            scene_buffer = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_ALLOC_HOST_PTR | CL_MEM_COPY_HOST_PTR, scene_data.size(), scene_data.data(), &err);
            checkError(err, __FILE__, __LINE__);

            mat_buffer = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_ALLOC_HOST_PTR | CL_MEM_COPY_HOST_PTR, mat_data.size(), mat_data.data(), &err);
            checkError(err, __FILE__, __LINE__);
        }

    }

    void CL_context::setupPlatforms()
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
        CL_context::Vendor vd = mapPlatformToVendor(vendor);

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
            CL_context::Vendor temp = mapPlatformToVendor(platform_list[i].vendor);
            if (temp == vd)
            {
                target_platform = platform_list[i];
                plat_idx = i;
            }
        }

        std::cout << "\nSelected Platform " << plat_idx << std::endl;
    }

    void CL_context::setupDevices(cl_context_properties* properties)
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

    CL_context::Vendor CL_context::mapPlatformToVendor(std::string str)
    {
        CL_context::Vendor vd;
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

    void CL_context::Platform::loadInfo(cl_platform_id plat_id)
    {
        platform_id = plat_id;
        {
            size_t len = 0;
            clGetPlatformInfo(platform_id, CL_PLATFORM_NAME, 0, NULL, &len);
            char arr[len];
            clGetPlatformInfo(platform_id, CL_PLATFORM_NAME, len, arr, NULL);
            name = std::string(arr);
        }

        {
            size_t len = 0;
            clGetPlatformInfo(platform_id, CL_PLATFORM_VENDOR, 0, NULL, &len);
            char arr[len];
            clGetPlatformInfo(platform_id, CL_PLATFORM_VENDOR, len, arr, NULL);
            vendor = std::string(arr);
        }

        {
            size_t len = 0;
            clGetPlatformInfo(platform_id, CL_PLATFORM_VERSION, 0, NULL, &len);
            char arr[len];
            clGetPlatformInfo(platform_id, CL_PLATFORM_VERSION, len, arr, NULL);
            version = std::string(arr);
        }

        {
            size_t len = 0;
            clGetPlatformInfo(platform_id, CL_PLATFORM_PROFILE, 0 , NULL, &len);
            char arr[len];
            clGetPlatformInfo(platform_id, CL_PLATFORM_PROFILE, len, arr, NULL);
            profile = std::string(arr);
        }
    }

    void CL_context::Platform::displayInfo(int i)
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

    void CL_context::Device::loadInfo(cl_device_id dev_id)
    {
        device_id = dev_id;
        size_t len;
        {
            clGetDeviceInfo(device_id, CL_DEVICE_NAME, 0, NULL, &len);
            char arr[len];
            clGetDeviceInfo(device_id, CL_DEVICE_NAME, len, arr, NULL);
            name = std::string(arr);
        }

        {
            clGetDeviceInfo(device_id, CL_DEVICE_VERSION, 0, NULL, &len);
            char arr[len];
            clGetDeviceInfo(device_id, CL_DEVICE_VERSION, len, arr, NULL);
            device_ver = std::string(arr);
        }

        {
            clGetDeviceInfo(device_id, CL_DRIVER_VERSION, 0, NULL, &len);
            char arr[len];
            clGetDeviceInfo(device_id, CL_DRIVER_VERSION, len, arr, NULL);
            driver_ver = std::string(arr);
        }

        {
            clGetDeviceInfo(device_id, CL_DEVICE_OPENCL_C_VERSION, 0, NULL, &len);
            char arr[len];
            clGetDeviceInfo(device_id, CL_DEVICE_OPENCL_C_VERSION, len, arr, NULL);
            device_openclC_ver = std::string(arr);
        }

        {
            clGetDeviceInfo(device_id, CL_DEVICE_EXTENSIONS, 0 , NULL, &len);
            char arr[len];
            clGetDeviceInfo(device_id, CL_DEVICE_EXTENSIONS, len, arr, NULL);
            ext = std::string(arr);
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

    void CL_context::Device::displayInfo(int i)
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

    void CL_context::checkError(cl_int err_code, std::string filename, int line_number)
    {

        std::string line = std::to_string(line_number);
        if(err_code >= 0)
            return;

        switch (err_code)
        {
            case -1: throw RuntimeError("CL_DEVICE_NOT_FOUND", filename, line);
            case -2: throw RuntimeError("CL_DEVICE_NOT_AVAILABLE", filename, line);
            case -3: throw RuntimeError("CL_COMPILER_NOT_AVAILABLE", filename, line);
            case -4: throw RuntimeError("CL_MEM_OBJECT_ALLOCATION_FAILURE", filename, line);
            case -5: throw RuntimeError("CL_OUT_OF_RESOURCES", filename, line);
            case -6: throw RuntimeError("CL_OUT_OF_HOST_MEMORY", filename, line);
            case -7: throw RuntimeError("CL_PROFILING_INFO_NOT_AVAILABLE", filename, line);
            case -8: throw RuntimeError("CL_MEM_COPY_OVERLAP", filename, line);
            case -9: throw RuntimeError("CL_IMAGE_FORMAT_MISMATCH", filename, line);
            case -10: throw RuntimeError("CL_IMAGE_FORMAT_NOT_SUPPORTED", filename, line);
            case -12: throw RuntimeError("CL_MAP_FAILURE", filename, line);
            case -13: throw RuntimeError("CL_MISALIGNED_SUB_BUFFER_OFFSET", filename, line);
            case -14: throw RuntimeError("CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST", filename, line);
            case -15: throw RuntimeError("CL_COMPILE_PROGRAM_FAILURE", filename, line);
            case -16: throw RuntimeError("CL_LINKER_NOT_AVAILABLE", filename, line);
            case -17: throw RuntimeError("CL_LINK_PROGRAM_FAILURE", filename, line);
            case -18: throw RuntimeError("CL_DEVICE_PARTITION_FAILED", filename, line);
            case -19: throw RuntimeError("CL_KERNEL_ARG_INFO_NOT_AVAILABLE", filename, line);
            case -30: throw RuntimeError("CL_INVALID_VALUE", filename, line);
            case -31: throw RuntimeError("CL_INVALID_DEVICE_TYPE", filename, line);
            case -32: throw RuntimeError("CL_INVALID_PLATFORM", filename, line);
            case -33: throw RuntimeError("CL_INVALID_DEVICE", filename, line);
            case -34: throw RuntimeError("CL_INVALID_CONTEXT", filename, line);
            case -35: throw RuntimeError("CL_INVALID_QUEUE_PROPERTIES", filename, line);
            case -36: throw RuntimeError("CL_INVALID_COMMAND_QUEUE", filename, line);
            case -37: throw RuntimeError("CL_INVALID_HOST_PTR", filename, line);
            case -38: throw RuntimeError("CL_INVALID_MEM_OBJECT", filename, line);
            case -39: throw RuntimeError("CL_INVALID_IMAGE_FORMAT_DESCRIPTOR", filename, line);
            case -40: throw RuntimeError("CL_INVALID_IMAGE_SIZE", filename, line);
            case -41: throw RuntimeError("CL_INVALID_SAMPLER", filename, line);
            case -42: throw RuntimeError("CL_INVALID_BINARY", filename, line);
            case -43: throw RuntimeError("CL_INVALID_BUILD_OPTIONS", filename, line);
            case -44: throw RuntimeError("CL_INVALID_PROGRAM", filename, line);
            case -45: throw RuntimeError("CL_INVALID_PROGRAM_EXECUTABLE", filename, line);
            case -46: throw RuntimeError("CL_INVALID_KERNEL_NAME", filename, line);
            case -47: throw RuntimeError("CL_INVALID_KERNEL_DEFINITION", filename, line);
            case -48: throw RuntimeError("CL_INVALID_KERNEL", filename, line);
            case -49: throw RuntimeError("CL_INVALID_ARG_INDEX", filename, line);
            case -50: throw RuntimeError("CL_INVALID_ARG_VALUE", filename, line);
            case -51: throw RuntimeError("CL_INVALID_ARG_SIZE", filename, line);
            case -52: throw RuntimeError("CL_INVALID_KERNEL_ARGS", filename, line);
            case -53: throw RuntimeError("CL_INVALID_WORK_DIMENSION", filename, line);
            case -54: throw RuntimeError("CL_INVALID_WORK_GROUP_SIZE", filename, line);
            case -55: throw RuntimeError("CL_INVALID_WORK_ITEM_SIZE", filename, line);
            case -56: throw RuntimeError("CL_INVALID_GLOBAL_OFFSET", filename, line);
            case -57: throw RuntimeError("CL_INVALID_EVENT_WAIT_LIST", filename, line);
            case -58: throw RuntimeError("CL_INVALID_EVENT", filename, line);
            case -59: throw RuntimeError("CL_INVALID_OPERATION", filename, line);
            case -60: throw RuntimeError("CL_INVALID_GL_OBJECT", filename, line);
            case -61: throw RuntimeError("CL_INVALID_BUFFER_SIZE", filename, line);
            case -62: throw RuntimeError("CL_INVALID_MIP_LEVEL", filename, line);
            case -63: throw RuntimeError("CL_INVALID_GLOBAL_WORK_SIZE", filename, line);
            case -64: throw RuntimeError("CL_INVALID_PROPERTY", filename, line);
            case -65: throw RuntimeError("CL_INVALID_IMAGE_DESCRIPTOR", filename, line);
            case -66: throw RuntimeError("CL_INVALID_COMPILER_OPTIONS", filename, line);
            case -67: throw RuntimeError("CL_INVALID_LINKER_OPTIONS", filename, line);
            case -68: throw RuntimeError("CL_INVALID_DEVICE_PARTITION_COUNT", filename, line);
            case -69: throw RuntimeError("CL_INVALID_PIPE_SIZE", filename, line);
            case -70: throw RuntimeError("CL_INVALID_DEVICE_QUEUE", filename, line);
            case -71: throw RuntimeError("CL_INVALID_SPEC_ID", filename, line);
            case -72: throw RuntimeError("CL_MAX_SIZE_RESTRICTION_EXCEEDED", filename, line);
            case -1002: throw RuntimeError("CL_INVALID_D3D10_DEVICE_KHR", filename, line);
            case -1003: throw RuntimeError("CL_INVALID_D3D10_RESOURCE_KHR", filename, line);
            case -1004: throw RuntimeError("CL_D3D10_RESOURCE_ALREADY_ACQUIRED_KHR", filename, line);
            case -1005: throw RuntimeError("CL_D3D10_RESOURCE_NOT_ACQUIRED_KHR", filename, line);
            case -1006: throw RuntimeError("CL_INVALID_D3D11_DEVICE_KHR", filename, line);
            case -1007: throw RuntimeError("CL_INVALID_D3D11_RESOURCE_KHR", filename, line);
            case -1008: throw RuntimeError("CL_D3D11_RESOURCE_ALREADY_ACQUIRED_KHR", filename, line);
            case -1009: throw RuntimeError("CL_D3D11_RESOURCE_NOT_ACQUIRED_KHR", filename, line);
            case -1010: throw RuntimeError("CL_INVALID_DX9_MEDIA_ADAPTER_KHR", filename, line);
            case -1011: throw RuntimeError("CL_INVALID_DX9_MEDIA_SURFACE_KHR", filename, line);
            case -1012: throw RuntimeError("CL_DX9_MEDIA_SURFACE_ALREADY_ACQUIRED_KHR", filename, line);
            case -1013: throw RuntimeError("CL_DX9_MEDIA_SURFACE_NOT_ACQUIRED_KHR", filename, line);
            case -1093: throw RuntimeError("CL_INVALID_EGL_OBJECT_KHR", filename, line);
            case -1092: throw RuntimeError("CL_EGL_RESOURCE_NOT_ACQUIRED_KHR", filename, line);
            case -1001: throw RuntimeError("CL_PLATFORM_NOT_FOUND_KHR", filename, line);
            case -1057: throw RuntimeError("CL_DEVICE_PARTITION_FAILED_EXT", filename, line);
            case -1058: throw RuntimeError("CL_INVALID_PARTITION_COUNT_EXT", filename, line);
            case -1059: throw RuntimeError("CL_INVALID_PARTITION_NAME_EXT", filename, line);
            case -1094: throw RuntimeError("CL_INVALID_ACCELERATOR_INTEL", filename, line);
            case -1095: throw RuntimeError("CL_INVALID_ACCELERATOR_TYPE_INTEL", filename, line);
            case -1096: throw RuntimeError("CL_INVALID_ACCELERATOR_DESCRIPTOR_INTEL", filename, line);
            case -1097: throw RuntimeError("CL_ACCELERATOR_TYPE_NOT_SUPPORTED_INTEL", filename, line);
            case -1000: throw RuntimeError("CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR", filename, line);
            case -1098: throw RuntimeError("CL_INVALID_VA_API_MEDIA_ADAPTER_INTEL", filename, line);
            case -1099: throw RuntimeError("CL_INVALID_VA_API_MEDIA_SURFACE_INTEL", filename, line);
            case -1100: throw RuntimeError("CL_VA_API_MEDIA_SURFACE_ALREADY_ACQUIRED_INTEL", filename, line);
            case -1101: throw RuntimeError("CL_VA_API_MEDIA_SURFACE_NOT_ACQUIRED_INTEL", filename, line);
            default: throw RuntimeError("CL_UNKNOWN_ERROR", filename, line);
        }
    }


}
