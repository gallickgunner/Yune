# Yune

Yune is a framework for a Physically based Renderer based on the GPU using OpenCL. It is mainly aimed towards young programmers and researchers who are looking to write their own Ray tracers, Path tracers and implementing other advanced techniques like Physically based Reflection models, etc. It provides the basic funtionality to open up a window, display and save Images that are processed by the GPU. We also provide some basic benchmarking functionality. *FPS, ms/frame, ms/kernel, total render time and samples per pixel (spp)* are updated in real-time. You can save the image at specific **spp** by providing the value in options. This helps in seeing performance gains if you are working on optimizing the code or comparing different implementations.

If you are familiar with [Nori](https://github.com/wjakob/nori), this gets easier. Yune is more like Nori on the GPU. As described earlier the host portion provides all the basic functionalities to open up a window, display the image and setup minimal buffers that would be required by a normal Pathtracer or Raytracer. What happens to the Image next depends on the Kernel which is upto the programmer that is **you**.

That being said as this is part of my and [@jabberwoky](https://github.com/jabberw0ky)'s bachelor thesis on Physically Based Rendering, we will be upgrading the GPU portion as well and keep adding new techniques as we study.

You can use this software in two ways.

1. Clone the project as it is, link it's dependencies manually and then start programming as if the whole project was coded by you. This may sound daunting but it's actually pretty easy and recommended. Since there are only a handful of files and if you spend a little bit of time you can easily understand how most of the things are setup. Then you can proceed to add features such as acceleration data structures etc.

2. You can use only the executable. You can then only do OpenCL programming in a separate file and mention it's name in *"yune-options.yun"* file. You won't need to recompile the project at all. This means you can keep on changing the kernel file or even use many different files with different algorithms without compiling at all provided that the new algorithms don't need change in host (CPU side) logic :)

Yune uses OpenCL-GL interoperability as the main core for processing images on the kernel and displaying them on the window. Aside from that it uses 

* [GLFW](https://github.com/glfw/glfw), for window and input managment.
* [GLAD](https://github.com/Dav1dde/glad) for OpenGL 3.x and higher windows.
* [Eigen](https://github.com/eigenteam/eigen-git-mirror) for basic Vector types and math.
* [FreeImage](http://freeimage.sourceforge.net) for saving screenshots.
* [NanoGUI](https://github.com/wjakob/nanogui) for basic widgets.

Note that you don't need many of these dependencies as NanoGUI already has GLFW, GLAD, EIGEN in it. So in actuality all you need is NanoGUI and FreeImage. 

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
I haven't tested this on other platforms except Windows but the code was written keeping platform independency in mind. If anybody is willing to check it on other platforms I'd be grateful. I haven't added support for build system like Cmake yet. If anybody still wants to compile it you can fetch NanoGUI and FreeImage libraries and link it up with this project as is in your IDE. And make sure to include these global #defines (option somewhere in your IDE and project's build options (if you use CodeBlocks).

CL_MINIMUM_OPENCL_VERSION=xxx<br/>
CL_TARGET_OPENCL_VERSION=xxx<br/>
EIGEN_DONT_ALIGN



Here is a picture obtained by Uni-directional Pathtracing with Next Event Estimation and using Modified Phong BRDF at 4000 spp.  

<img src="https://r0pikq.bn.files.1drv.com/y4mhWXu4VSbDT-gNofzyb9R4P_t8n56ygrisW0bLnhN2owntA6OlCE4H84AYsprxs03moM2zI4s_aIlOLyM--1jH10RPwUBbhMxbmqxqWOl63V1zOi4Le258LztUiBIa0AIRSskmfLFxWJzA6jw8L2zYsQoR2J_6aaDzBqUCRI1dW9bePXgrjliRY3WnbzL6YMcp2jjFFOgwYX3oVnoXglZgQ?cropmode=none"/>

<img src="https://r0mthq.bn.files.1drv.com/y4mw4XP2D_dsdDyREYi1Y37ScaDS_0utfRuOKBBccQ8THZnHLFWtjL7c20AqXM1dLZDG_WzLadF-tHkTASrFVt-Acb3RqCBb0aLZbptgHQKvU6PQ0re6RekumIWa2ti6Q6jrOSNazChMYmxuTzkygHKucXjzZaRH-GmiPruhnu5SwQ0bNkxtHEsV6FcBmWthpUtxHCDoBFirABlgiFZJPhh4Q?width=1300&height=767&cropmode=none"/>

### Added a release. Check it out.




 
