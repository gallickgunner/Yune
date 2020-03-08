![Yune Results2](https://github.com/gallickgunner/Yune/blob/Pictures/Images/5kspp-cb.jpg?raw=true)
![teapot](https://github.com/gallickgunner/Yune/blob/Pictures/Images/teapot-2kspp-refr.jpg?raw=true)
![teapot-reflective](https://github.com/gallickgunner/Yune/blob/Pictures/Images/teapot-refl-2500pp.jpg?raw=true)
![cbwith-refractive-sphere](https://github.com/gallickgunner/Yune/blob/Pictures/Images/5kspp-CB-Sphere.jpg?raw=true)


# Yune

Yune is a framework for writing interactive raytracers, pathracers based on the GPU using OpenCL. It is mainly aimed towards young programmers and researchers who are looking to write their own Ray tracers, Path tracers and implementing other advanced techniques like Physically based Reflection models, etc but don't want to deal with the whole boilerplate code to set things up. It provides the basic funtionality to open up a window, display and save Images that are processed by the GPU. We also provide some basic profiling functionality. *FPS, ms/frame, ms/kernel, total render time and samples per pixel (spp)* are updated in real-time. You can save the image at specific **spp** by providing the value in options. This helps in seeing performance gains if you are working on optimizing the code or comparing different implementations.

If you are familiar with [Nori](https://github.com/wjakob/nori), this gets easier. Yune is more like Nori on the GPU. As described earlier the host portion provides all the basic functionalities to open up a window, display the image and setup minimal buffers that would be required by a normal Pathtracer or Raytracer. What happens to the Image next depends on the Kernel which is upto the programmer that is **you**.

That being said as this is part of our bachelor thesis on Physically Based Rendering, we will be upgrading the GPU portion as well and keep adding new techniques as we study.

You can use this software in two ways.

1. Clone the project as it is, link it's dependencies manually and then start programming as if the whole project was coded by you. This may sound daunting but it's actually pretty easy and recommended. Since there are only a handful of files and if you spend a little bit of time you can easily understand how most of the things are setup. Then you can proceed to add features such as acceleration data structures etc.

2. You can use only the executable. You can then only do OpenCL programming in a separate file and mention it's name in *"yune-options.yun"* file. You won't need to recompile the project at all. This means you can keep on changing the kernel file or even use many different files with different algorithms without compiling at all provided that the new algorithms don't need change in host (CPU side) logic :)

Yune uses OpenCL-GL interoperability as the main core for processing images on the kernel and displaying them on the window. Aside from that it uses 

* [GLFW](https://github.com/glfw/glfw), for window and input managment.
* [GLAD](https://github.com/Dav1dde/glad) for OpenGL 3.x and higher windows.
* [Eigen](https://github.com/eigenteam/eigen-git-mirror) for basic Vector types and math.
* [FreeImage](http://freeimage.sourceforge.net) for saving screenshots.
* [NanoGUI](https://github.com/wjakob/nanogui) for basic widgets.

Note that you don't need many of these dependencies as NanoGUI already has GLFW, GLAD, EIGEN in it. So in actuality all you need is NanoGUI and FreeImage. You need an OpenCL 1.1 or higher compatible CPU/GPU that also supports the CL extension *cl_khr_gl_sharing* (CL-GL interoperability) You don't have to fret over it though as the program will tell you if the device isn't supported. 

I haven't added support for build system like Cmake yet. If anybody still wants to compile, I've written some steps in the [wiki](https://github.com/gallickgunner/Yune/wiki/Getting-Started). Check that out.

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

### Added support for triangles. 

Yune now supports triangles. If you'd like to import any models into the screen you have to first convert them into .rtt format. Make sure the file is .OBJ and has grouping on. Afterwards use the Yune - Parser to convert the desired file into .rtt format which is supported by the renderer. Provide the name of the file in the main and it will load up automatically when you start the renderer. 

A material file is required in order to view imported models. In order to make one follow the template provided on how to make .pbm file. Once you have made the .pbm file you can assign materials in that file to different groups of the model by providing the matID at the start of the group in .rtt file.

Use the link below to download the parser.
[ Yune-OBJ-Parser ](https://github.com/jabberw0ky/Yune-OBJ-Parser)

## TODO

- Revamp UI by using Dear-ImGui
- Use ImGui to get rid of the ***yune-options.yun*** file. All configurable parameters should be through UI.
- Provide more profiling and benchmarking functionality like comparison of Images with reference Images from other Renderers like Mitsuba, etc.
- Remove custom geometry files and add support for directly reading from .OBJ
- Add support to leave out post-processing kernel.
- Compare our own images with Reference ones from Mitsuba.* (personal)
