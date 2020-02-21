/******************************************************************************
 *  This file is part of Yune".
 *
 *  Copyright (C) 2018 by Umair Ahmed and Syed Moiz Hussain.
 *
 *  "Yune" is a framework for a Physically Based Renderer. It's aimed at young
 *  researchers trying to implement Physically Based Rendering techniques.
 *
 *  "Yune" is a free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  "Yune" is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 ******************************************************************************/

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
        std::vector<Triangle> scene_data;
        std::vector<Material> mat_data;
        //cl_float8 test;

        Renderer rendr;
        Camera main_cam(90);
        Scene render_scene(main_cam);

        render_scene.loadModel("CornelBox_V3.rtt",scene_data,"Material.pbm",mat_data);
        rendr.setup(&render_scene, scene_data, mat_data);
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
