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

#include <RuntimeError.h>
#include <CL_context.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <string>
#include <limits>

namespace helion_render
{
    CL_context::Platform::Platform()
    {
    }

    CL_context::Platform::~Platform()
    {
    }

    CL_context::Device::Device() :  max_workitem_size(20)
    {
    }

    CL_context::Device::~Device()
    {
    }

    CL_context::CL_context()
    {
        kernel = NULL;
        comm_queue = NULL;
        image_buffers[0] = NULL;
        image_buffers[1] = NULL;
        scene_buffer = NULL;
        camera_buffer = NULL;
        program = NULL;
        context = NULL;
        //ctor
    }

    CL_context::~CL_context()
    {
        if(kernel)
            clReleaseKernel(kernel);
        if(program)
            clReleaseProgram(program);
        if(comm_queue)
            clReleaseCommandQueue(comm_queue);
        if(image_buffers[0])
            clReleaseMemObject(image_buffers[0]);
        if(image_buffers[1])
            clReleaseMemObject(image_buffers[1]);
        if(scene_buffer)
            clReleaseMemObject(scene_buffer);
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

    void CL_context::createProgram(std::string file_name, std::string kernel_name, std::string cl_ver)
    {
        // Release Previous Program and kernel if any.
        if(kernel)
            clReleaseKernel(kernel);
        if(program)
            clReleaseProgram(program);

        cl_int err=0;
        std::ifstream file(file_name);
        std::string data(
                          std::istreambuf_iterator<char>(file),
                         (std::istreambuf_iterator<char>()          )
                        );
        const char* str = data.c_str();
        program = clCreateProgramWithSource(context, 1, &str, NULL, &err);

        err = clBuildProgram(program, 1, &target_device.device_id, cl_ver.data(), NULL, NULL);
        if(err < 0)
        {
            std::cout << "\nProgram failed to build.." << std::endl;

            size_t log_size;
            err = clGetProgramBuildInfo(program, target_device.device_id, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
            checkError(err, __FILE__, __LINE__ - 1);

            char log[log_size];
            err = clGetProgramBuildInfo(program, target_device.device_id, CL_PROGRAM_BUILD_LOG, log_size, log, NULL);
            checkError(err, __FILE__, __LINE__ - 1);
            throw RuntimeError("\n" + std::string(log) + "\n");
        }

        kernel = clCreateKernel(program, kernel_name.data(), &err);
        checkError(err, __FILE__, __LINE__ - 1);

        // Set Kernel memory and workgroup requirements. Check if Kernel's requirements exceed device capabilities.
        cl_ulong kernel_local_mem_size;
        clGetKernelWorkGroupInfo(kernel, target_device.device_id, CL_KERNEL_WORK_GROUP_SIZE, sizeof(size_t), &kernel_workgroup_size, NULL);
        clGetKernelWorkGroupInfo(kernel, target_device.device_id, CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE, sizeof(size_t), &preferred_workgroup_multiple, NULL);
        clGetKernelWorkGroupInfo(kernel, target_device.device_id, CL_KERNEL_LOCAL_MEM_SIZE, sizeof(cl_ulong), &kernel_local_mem_size, NULL);

        std::cout << "\nThe currently selected Device has the following features.." << std::endl;
        std::cout << "Max Compute Units: " << target_device.compute_units << "\n"
                  << "Max Workgroup Size: " << target_device.wg_size << "\n"
                  << "Max WorkItem Size: " << target_device.max_workitem_size[0] << ", " << target_device.max_workitem_size[1] << "\n"
                  << "Max Local Memory Size: " << target_device.local_mem_size/(1024) << " KB" << "\n" << std::endl;

        std::cout << "\nThe Kernel has the following features.." << std::endl;
        std::cout << "Kernel Workgroup Size: " << kernel_workgroup_size << "\n"
                  << "Preferred WorkGroup Multiple Size: " <<  preferred_workgroup_multiple << "\n"
                  << "Kernel Local Memory Required: " << kernel_local_mem_size/(1024) << " KB" << "\n" << std::endl;

        if(kernel_local_mem_size > target_device.local_mem_size)
            std::cout << "Kernel local memory requirement exceeds Device's local memory.\nProgram may crash during kernel processing.\n" << std::endl;
    }

    void CL_context::setupBuffers(GLuint* rbo_IDs, std::vector<float>& scene_data, std::vector<float>& cam_data)
    {
        cl_int err = 0;
        //Check if buffer size exceed Device's global memory.
        if( (scene_data.size() + cam_data.size() ) > target_device.global_mem_size)
            std::cout << "Buffer size exceeds Device's global memory size.\nProgram may crash during kernel processing.\n" << std::endl;

        //Setup 2 Image Objects over 2 GL RenderBuffer Objects and  Buffer Objects containing scene data and camera data.
        image_buffers[0] = clCreateFromGLRenderbuffer(context, CL_MEM_READ_WRITE, rbo_IDs[0], &err);
        checkError(err, __FILE__, __LINE__ - 1);

        image_buffers[1] = clCreateFromGLRenderbuffer(context, CL_MEM_READ_WRITE, rbo_IDs[1], &err);
        checkError(err, __FILE__, __LINE__ - 1);

        //scene_buffer = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, scene_data.size(), scene_data.data(), &err);
        //checkError(err, __FILE__, __LINE__);

        camera_buffer = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_ALLOC_HOST_PTR | CL_MEM_COPY_HOST_PTR, cam_data.size() * sizeof(cl_float), cam_data.data(), &err);
        checkError(err, __FILE__, __LINE__ - 1);
    }

    void CL_context::setupPlatforms()
    {
        int plat_choice;
        cl_uint num_plat =0, err=0;
        clGetPlatformIDs(5, NULL, &num_plat);
        if(num_plat == 0)
            throw std::runtime_error("Failed to detect any platform.");

        // List of OpenCL platforms.
        std::vector<cl_platform_id> platforms(num_plat);
        std::vector<Platform> platform_list;

        err = clGetPlatformIDs(num_plat, platforms.data(), NULL);
        checkError(err, __FILE__, __LINE__ - 1);

        std::cout << platforms.size() << " OpenCL platform(s) detected..\n" << std::endl;
        std::cout << "*PLATFORM INFORMATION*" << std::endl;

        for(int i = 0; i < platforms.size(); i++)
        {
            platform_list.push_back(Platform());
            platform_list[i].loadInfo(platforms[i]);
            platform_list[i].displayInfo(i);
        }

        // If more than 1 platform detected, ask from user...
        if(platforms.size() > 1)
        {
            std::cout << "\nPress any number between " << 0 << " and " << platforms.size()-1
                      << " to choose a platform" << std::endl;
            std::cin >> plat_choice;
            target_platform = platform_list[plat_choice];
        }
        else
        {
            std::cout << "\nNo more Platforms present. Selected Platform 0.\nPress Enter to continue..." << std::endl;
            std::cin.ignore();
            target_platform = platform_list[0];
        }
    }

    void CL_context::setupDevices(cl_context_properties* properties)
    {
        int dev_choice = 0; // default to first entry
        size_t num_devices;
        cl_int err;
        // Load extension
        clGetGLContextInfoKHR_fn clGetGLContextInfoKHR = NULL;

        #if CL_TARGET_OPENCL_VERSION > 110
        clGetGLContextInfoKHR = (clGetGLContextInfoKHR_fn) clGetExtensionFunctionAddressForPlatform(target_platform.platform_id, "clGetGLContextInfoKHR");
        #else
        clGetGLContextInfoKHR = (clGetGLContextInfoKHR_fn) clGetExtensionFunctionAddress("clGetGLContextInfoKHR");
        #endif
        if(!clGetGLContextInfoKHR)
            throw std::runtime_error("\"clGetGLContextInfoKHR\" Function failed to load.");

        err = clGetGLContextInfoKHR(properties, CL_DEVICES_FOR_GL_CONTEXT_KHR, 0, NULL, &num_devices);
        checkError(err, __FILE__, __LINE__ - 1);

        num_devices = num_devices/sizeof(cl_device_id);

        if(num_devices == 0)
        {
            std::string error = "\nEither there is no device available or they don't support \"";
            error = error + CL_GL_SHARING_EXT + "\" extension.";
            throw std::runtime_error(error);
        }

        std::vector<cl_device_id> devices(num_devices);
        std::vector<Device> device_list;

        err = clGetGLContextInfoKHR(properties, CL_DEVICES_FOR_GL_CONTEXT_KHR, sizeof(cl_device_id)*num_devices, devices.data(), NULL);
        checkError(err, __FILE__, __LINE__ - 1);

        std::cout << "\n" << devices.size() << " OpenCL Device(s) detected..\n" << std::endl;
        std::cout << "*DEVICE INFORMATION*" << std::endl;
        for(int i = 0; i < devices.size(); i++)
        {
            device_list.push_back(Device());
            device_list[i].loadInfo(devices[i]);
            device_list[i].displayInfo(i);
        }

        if(devices.size() > 1)
        {
            std::cout << "\nPress any number between " << 0 << " and " << devices.size()-1
                      << " to choose a device" << std::endl;
            std::cin >> dev_choice;
            target_device = device_list[dev_choice];
        }
        else
        {
            std::cout << "\nNo more Devices present. Selected Device 0.\nPress Enter to continue..." << std::endl;
            std::cin.ignore();
            std::cin.ignore();
            target_device = device_list[0];
        }
    }

    void CL_context::Platform::loadInfo(cl_platform_id plat_id)
    {
        platform_id = plat_id;
        size_t len = 0;

        {
            clGetPlatformInfo(platform_id, CL_PLATFORM_NAME, 0, NULL, &len);
            char arr[len];
            clGetPlatformInfo(platform_id, CL_PLATFORM_NAME, len, arr, NULL);
            name = std::string(arr);
        }

        {
            clGetPlatformInfo(platform_id, CL_PLATFORM_VENDOR, 0, NULL, &len);
            char arr[len];
            clGetPlatformInfo(platform_id, CL_PLATFORM_VENDOR, len, arr, NULL);
            vendor = std::string(arr);
        }

        {
            clGetPlatformInfo(platform_id, CL_PLATFORM_VERSION, 0, NULL, &len);
            char arr[len];
            clGetPlatformInfo(platform_id, CL_PLATFORM_VERSION, len, arr, NULL);
            version = std::string(arr);
        }

        {
            clGetPlatformInfo(platform_id, CL_PLATFORM_PROFILE, 0 , NULL, &len);
            char arr[len];
            clGetPlatformInfo(platform_id, CL_PLATFORM_PROFILE, len, arr, NULL);
            profile = std::string(arr);
        }
    }

    void CL_context::Platform::displayInfo(int i)
    {
        std::cout << "-----------------------------------------" << i << "--------------------------------------\n"
                  << std::left << std::setw(35) << "Platform Name" << ": " << name << "\n"
                  << std::left << std::setw(35) << "Vendor Name" << ": " << vendor << "\n"
                  << std::left << std::setw(35) << "OpenCL Profile" << ": " << profile << "\n"
                  << std::left << std::setw(35) << "OpenCL Version Supported" << ": " << version << "\n"
                  << "--------------------------------------------------------------------------------"
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
        std::string type;
        if(device_type & CL_DEVICE_TYPE_CPU)
            type = "CPU";
        else if(device_type & CL_DEVICE_TYPE_GPU)
            type = "GPU";
        else if(device_type & CL_DEVICE_TYPE_ACCELERATOR)
            type = "ACCELERATOR";


        std::string ext1("cl_khr_gl_event");
        if(ext.find(ext1) != std::string::npos)
            ext_supported = true;

        std::cout << "-----------------------------------------" << i << "--------------------------------------\n"
                  << std::left << std::setw(35) << "Device Name" << ": " << name << "\n"
                  << std::left << std::setw(35) << "Device Type" << ": " << type << "\n"
                  << std::left << std::setw(35) << "OpenCL Version Supported" <<  ": " << device_ver << "\n"
                  << std::left << std::setw(35) << "OpenCL Software Driver Version" << ": " << driver_ver << "\n"
                  << std::left << std::setw(35) << "OpenCL-C Version Supported" << ": " <<  device_openclC_ver << "\n"
                  << std::left << std::setw(35) << "Device Available" << ": " << (availability ? "Yes" : "No") << "\n"
                  << std::left << std::setw(35) << "SP Floating Point Supported" << ": " << ( (fp_support & (CL_FP_ROUND_TO_NEAREST|CL_FP_INF_NAN) ) ? "Yes" : "No") << "\n"
                  << std::left << std::setw(35) << "Max Compute Units" << ": " << compute_units << "\n"
                  << std::left << std::setw(35) << "Max Workgroup Size" << ": " << wg_size << "\n"
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
