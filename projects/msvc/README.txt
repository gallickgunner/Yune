Yune - Visual Studio project
============================

Created using Visual Studio 2017 (v15.9.17) on Windows10 x64

Expects dependencies installed using vcpkg (https://github.com/microsoft/vcpkg).

-------------------------------------------------------------------------

Notes from vcpkg install console:

> The package glad:x64-windows provides CMake targets:
> 
>     find_package(glad CONFIG REQUIRED)
>     target_link_libraries(main PRIVATE glad::glad)

> The package glfw3:x64-windows provides CMake targets:
> 
>     find_package(glfw3 CONFIG REQUIRED)
>     target_link_libraries(main PRIVATE glfw)

> The package glm:x64-windows provides CMake targets:
> 
>     find_package(glm CONFIG REQUIRED)
>     target_link_libraries(main PRIVATE glm)

> The package opencl is compatible with built-in CMake targets via CMake v3.6 and prior syntax
> 
>     find_package(OpenCL REQUIRED)
>     target_link_libraries(main PRIVATE ${OpenCL_LIBRARIES})
>     target_include_directories(main PRIVATE ${OpenCL_INCLUDE_DIRS})
> 
> and the CMake v3.7 and beyond imported target syntax
> 
>     find_package(OpenCL REQUIRED)
>     target_link_libraries(main PRIVATE OpenCL::OpenCL)
> 
> This package is only an OpenCL SDK. To actually run OpenCL code you also need to install an implementation.

