#include <iostream>
#include <string>
#include <vector>
#include "RendererGUI.h"

using namespace yune;

int main()
{
    try
    {
        RendererGUI renderer;
        renderer.run();
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
