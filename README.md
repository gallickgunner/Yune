*Editor-Screen. Simple Primitives using Oren-Nayar for diffuse and Phong for glossy materials.*
![Yune Results2](https://github.com/gallickgunner/Yune/blob/Pictures/Images/editor.jpg?raw=true)

*Utah teapot rendered using transmissive material.*
![teapot](https://github.com/gallickgunner/Yune/blob/Pictures/Images/teapot-2kspp-refr.jpg?raw=true)

*Ico Sphere rendered using transmissive material.*
![cbwith-refractive-sphere](https://github.com/gallickgunner/Yune/blob/Pictures/Images/5kspp-CB-Sphere.jpg?raw=true)


# Yune

Yune started off as a personal toy pathtracer on the GPU using OpenCL. However, we've tried our best to mature it into a form of an educational raytracer/pathtracer. It's a framework for writing interactive raytracers, pathracers based on the GPU using OpenCL. It is mainly aimed towards young programmers and researchers who are looking to write their own Ray tracers, Path tracers and implementing other advanced techniques like Physically based Reflection models, etc but don't want to deal with the whole boilerplate code to set things up. It provides the basic funtionality to open up a window, display and save Images. We also provide some basic profiling functionality. *FPS, ms/frame, ms/kernel, total render time and samples per pixel (spp)* are updated in real-time. You can save the image at specific **spp** by providing the value in options. Although not something extravagant, it can help in seeing performance gains if you are working on optimizing the code or comparing different implementations, reflection models, etc.

If you are familiar with [Nori](https://github.com/wjakob/nori), this gets easier. Yune is more like a dumbed down Nori but on the GPU. As described earlier the host portion provides all the basic functionalities to open up a window, display the image and setup minimal buffers that would be required by a normal Pathtracer or Raytracer. What happens to the Image next, depends on the Kernel which is upto the programmer that is **you**.

That being said as this was part of our bachelor thesis on Physically Based Rendering and our personal project, we will be upgrading the GPU portion as well and keep adding new techniques as we study.

You can use this software in two ways.

1. Clone the project as it is, link it's dependencies manually and then start programming as if the whole project was coded by you. This may sound daunting but it's actually pretty easy and recommended if you want to mature it into something else or implement multipass kernels, etc. 

2. You can use only the executable. You can then only do OpenCL programming in a separate file and load it through GUI. This means you can keep on changing the kernel file, material files, or even use many different files with different algorithms without compiling at all provided that the new algorithms don't need change in host (CPU side) logic :)

Yune uses OpenCL-GL interoperability as the main core for processing images on the kernel and displaying them on the window. Aside from that it uses 

* [GLFW](https://github.com/glfw/glfw), for window and input managment.
* [GLAD](https://github.com/Dav1dde/glad) for OpenGL 3.x and higher windows.
* [GLM](https://github.com/g-truc/glm) for basic Vector types and math.
* [STB Image](https://github.com/nothings/stb) for saving screenshots.
* [Dear-ImGui](https://github.com/ocornut/imgui) for GUI and basic widgets.

You need an OpenCL 1.1 or higher compatible CPU/GPU that also supports the CL extension *cl_khr_gl_sharing* (CL-GL interoperability) You don't have to fret over it though as the extension is supported by many old GPUs as well. The program will also list down whether the extension is available or not. 

I haven't added support for build system like Cmake yet. If anybody still wants to compile, I've written some steps in the [wiki](https://github.com/gallickgunner/Yune/wiki/Getting-Started). 

***Check WIKI for guides regarding how to load model files.***

Currently Implemented features,
* Next Event Estimation aka Explicit direct light sampling
* Russian Roulette path termination
* Modified Phong BRDF (1994 - Lafortune)
* Multiple Light Source heuristic (chooses a single light source out of multiple ones)
* Multiple Importance Sampling
* Support for saving HDR Images
* Tone mapping and Gamma Correction
* Naive Bidirectional Pathtracing as in Lafortune's paper
* SAH-based BVH

#### Check the [RESOURCES.MD](https://github.com/gallickgunner/Yune/blob/master/RESOURCES.md) file for a list of reference material (research papers and books) used.

Watch it in action below, (click for full video)

[![Demo](https://i.imgur.com/jKqjYut.gif)](https://www.youtube.com/watch?v=PrbROGU0ztE)

## TODO

- ~~Revamp UI by using Dear-ImGui~~
- ~~Use ImGui to get rid of the ***yune-options.yun*** file. All configurable parameters should be through UI.~~
- Provide more profiling and benchmarking functionality like comparison of Images with reference Images from other Renderers like Mitsuba, etc.
- ~~Remove custom geometry files and add support for directly reading from .OBJ~~
- ~~Add support to leave out post-processing kernel.~~
- Compare our own images with Reference ones from Mitsuba.* (personal)
