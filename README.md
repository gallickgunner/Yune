![Yune Results2](https://user-images.githubusercontent.com/40864140/75049511-2cf42900-547f-11ea-81d9-c0c548967cbc.png)

![Yune Results](https://user-images.githubusercontent.com/40864140/75049866-c4597c00-547f-11ea-94d1-5d784a40fcd7.png)

![Bunny](https://user-images.githubusercontent.com/40864140/75080658-78c7c200-54c1-11ea-8ffa-d572f1d18ea4.png)

>*Results obtained from BDPT at 70 SPP*

# Yune

Yune is a framework for a Physically based Renderer based on the GPU using OpenCL. It is mainly aimed towards young programmers and researchers who are looking to write their own Ray tracers, Path tracers and implementing other advanced techniques like Physically based Reflection models, etc. It provides the basic funtionality to open up a window, display and save Images that are processed by the GPU. We also provide some basic profiling functionality. *FPS, ms/frame, ms/kernel, total render time and samples per pixel (spp)* are updated in real-time. You can save the image at specific **spp** by providing the value in options. This helps in seeing performance gains if you are working on optimizing the code or comparing different implementations.

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

---
If you are still confused whether this is up for you here are a few points to clear your confusion.

>**I am not a programmer and I intend to use this to see pretty images.**

Nope, you are at the wrong place. 
<br/>

>**I am a programmer/researcher and I intend to implement some rendering technique or render a scene.**

Yes, you are at the right place. You can either compile the whole project yourself or just use the "exe" and just do programming in the kernel file. If you are implementing rendering techniqeus this will work as long as your new algorithms won't require changes in host logic. For example multi-pass algoithms may require enqueuing more than 1 kernel. We still haven't generalized it yet.
<br/>
<br/>

 ---
I haven't tested this on other platforms except Windows but the code was written keeping platform independency in mind. If anybody is willing to check it on other platforms I'd be grateful. I haven't added support for build system like Cmake yet. If anybody still wants to compile, I've written some steps in the [wiki](https://github.com/gallickgunner/Yune/wiki/Getting-Started). Check that out.

Currently Implemented features,
* Next Event Estimation aka Explicit direct light sampling
* Russian Roulette path termination
* Modified Phong BRDF (1994 - Lafortune)
* Multiple Light Source heuristic (chooses a single light source out of multiple ones)
* Multiple Importance Sampling
* Support for saving HDR Images
* Tone mapping and Gamma Correction

Watch it in action below, (click for full video)

[![Demo](https://i.imgur.com/jKqjYut.gif)](https://www.youtube.com/watch?v=PrbROGU0ztE)

### Added support for triangles and converted from UDPT to BDPT. 

Yune now supports triangles and is a fully functional BDPT. If you'd like to import any models into the screen you have to first convert them into .rtt format. Make sure the file is .OBJ and has grouping on. Afterwards use the Yune - Parser to convert the desired file into .rtt format which is supported by the renderer. Provide the name of the file in the main and it will load up automatically when you start the renderer. 

A material file is required in order to view imported models. In order to make one follow the template provided on how to make .pbm file. Once you have made the .pbm file you can assign materials in that file to different groups of the model by providing the matID at the start of the group in .rtt file.

Use the link below to download the parser.

[ Yune-OBJ-Parser ](https://github.com/jabberw0ky/Yune-OBJ-Parser)

Currently Yune supports unidirectional pathtracer with primitives, unidirectional path tracer with triangles and bidirectional pathtracer with triangles.

More Updates coming soon :) 




 
