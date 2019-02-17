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

#include <Scene.h>
#include <RuntimeError.h>
#include <Eigen_typedefs.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <map>

namespace yune
{
    Scene::Scene()
    {
        //ctor
    }

    Scene::Scene(Camera cam) : main_camera(cam)
    {

    }

    Scene::~Scene()
    {

    }

    void Scene::loadModel(std::string objfile, std::vector<Triangle>& scene_data, std::string matfile, std::vector<Material>& mat_data)
    {
        if(objfile.substr(objfile.length()-4, objfile.length()) != ".rtt")
            throw RuntimeError("Invalid Object file. Please provide an object file with extension \".rtt\" ");
        else if(matfile.substr(matfile.length()-4, matfile.length()) != ".pbm")
            throw RuntimeError("Invalid Material file. Please provide a material file with extension \".pbm\" ");

        std::fstream file;
        std::map<int, int> mat_index;

        file.open(matfile);

        if(file.is_open())
        {
            std::string extract, line;
            cl_float x,y,z,w = 1.0f;

            while(getline(file, line))
            {
                std::stringstream ss;
                // skip comments and empty lines
                if(line[0] == '#' || line.empty())
                    continue;

                ss.str(line);
                ss >> extract;

                if(extract == "id")
                {
                    int id;
                    ss >> id;
                    mat_index[id] = mat_data.size();
                    mat_data.push_back(Material());
                }
                else if(extract == "ke")
                {
                    ss >> x >> y >> z;
                    mat_data.back().emissive = {x,y,z,w};
                }
                else if(extract == "kd")
                {
                    ss >> x >> y >> z;
                    mat_data.back().diff = {x,y,z,w};
                }
                else if(extract == "ks")
                {
                    ss >> x >> y >> z;
                    mat_data.back().spec = {x,y,z,w};
                }
                else if(extract == "ior")
                    ss >> mat_data.back().ior;
                else if(extract == "alpha_x")
                    ss >> mat_data.back().alpha_x;
                else if(extract == "alpha_y")
                    ss >> mat_data.back().alpha_y;
                else if(extract == "mirror")
                    ss >> mat_data.back().is_specular;
                else if(extract == "transmissive")
                    ss >> mat_data.back().is_transmissive;
            }
            file.close();
        }
        else
            throw RuntimeError("Error opening material file...");

        file.open(objfile);
        if(file.is_open())
        {
            cl_float x,y,z,w = 1.f;
            int i = 0, j = 0, mat_id = 0;
            std::string extract, line;

            while(getline(file, line))
            {
                std::stringstream ss;

                // skip comments and empty lines
                if(line[0] == '#' || line.empty())
                    continue;

                ss.str(line);
                ss >> extract;

                if(extract == "f")
                {
                    scene_data.push_back(Triangle());
                    scene_data.back().matID = mat_id;
                }

                else if (extract == "matID")
                {
                    ss >> mat_id;
                    mat_id = mat_index[mat_id];
                }
                else if (extract == "v")
                {
                    i++;
                    w = 1.f;
                    ss >> x >> y >> z;

                    if(i == 1)
                        scene_data.back().v1 = {x,y,z,w};
                    else if(i == 2)
                        scene_data.back().v2 = {x,y,z,w};
                    else if (i == 3)
                    {
                        scene_data.back().v3 = {x,y,z,w};
                        i = 0;
                    }
                }
                else if (extract == "vn")
                {
                    j++;
                    w = 0.f;
                    ss >> x >> y >> z;

                    if(j == 1)
                        scene_data.back().vn1 = {x,y,z,w};
                    else if(j == 2)
                        scene_data.back().vn2 = {x,y,z,w};
                    else if (j == 3)
                    {
                        scene_data.back().vn3 = {x,y,z,w};
                        j = 0;
                    }
                }
            }
            file.close();
        }
        else
            throw RuntimeError("Error opening object file...");
    }

    void Scene::setBuffer()
    {
        int offset;
    }
}

