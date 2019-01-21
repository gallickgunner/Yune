#include <iostream>
#include <string>
#include <vector>
#include <RuntimeError.h>
#include <Renderer.h>



using namespace yune;

int main()
{
    try
    {
        Renderer rendr;
        Camera main_cam(90);
        std::vector<AreaLight> light_sources;
        light_sources.push_back(AreaLight(Vec4f(0,8,-1,1), Vec4f(1,1,1,0), Vec4f(1,1,1,0), 1.0f));
        Scene render_scene(main_cam, light_sources);
        rendr.setup(&render_scene);
        rendr.start();
    }
    catch(const std::exception& err)
    {
        std::cout << err.what() << std::endl;
        std::cout << "\nPress Enter to continue..." << std::endl;
        std::cin.ignore();
        return EXIT_FAILURE;
    }


    return EXIT_SUCCESS;
}
