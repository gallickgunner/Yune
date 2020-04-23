Yune - Building with Visual Studio
==================================

Use Visual Studio 2017 or later. Project was created using Visual Studio 2017 (v15.9.17)

To obtain dependencies, set up vcpkg (https://github.com/microsoft/vcpkg)
and install the following packages:
  - glad
  - glfw3
  - glm
  - opencl

After installing dependencies, open the solution (.sln),
select your platform (x86/x64) and hit F5 to build and run.
Dependencies should be located automatically via vcpkg integration.

Tested on Windows10 (x64)