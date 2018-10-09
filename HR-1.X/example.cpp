#include <iostream>
#include <string>
#include <vector>
#include <RuntimeError.h>
#include <Renderer.h>

using namespace helion_render;

int main()
{

    try
    {
        Renderer rendr;
        Camera main_cam(90);
        std::vector<AreaLight> light_sources;
        light_sources.push_back(AreaLight(Vec4f(0,8,-1,1), Vec4f(1,1,1,0), Vec4f(1,1,1,0), 1.0f));
        Scene render_scene(main_cam, light_sources);

        GL_context glfw_manager;
        CL_context cl_manager;

        glfw_manager.createWindow(640, 480, false);
        cl_manager.setup();
        cl_manager.createProgram("opencl_kernel.cl", "evaluatePixelRadiance", "-cl-std=CL1.1");

        rendr.setup(&glfw_manager, &cl_manager, &render_scene);
        size_t gws[2], lws[2];
        rendr.getWorkGroupSizes(gws, lws);
        rendr.start(gws, lws);
    }
    catch(RuntimeError& err)
    {
        std::cout << err.what() << std::endl;
        std::cout << "\nPress Enter to continue..." << std::endl;
        char x;
        std::cin >> x;
        return EXIT_FAILURE;
    }


    return EXIT_SUCCESS;
}
