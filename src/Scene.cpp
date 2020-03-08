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
#include <limits>

namespace yune
{
    Scene::Scene()
    {
        //ctor
    }

    Scene::Scene(Camera cam, BVH bvh) : main_camera(cam), bvh(bvh)
    {
    }

    Scene::~Scene()
    {
    }

    void Scene::loadModel(std::string objfile, std::string matfile)
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
            std::cout << "Reading Material File..." << std::endl;
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

        //Read Vertex Data
        file.open(objfile);
        if(file.is_open())
        {
            std::cout << "Reading Object File..." << std::endl;
            cl_float x,y,z,w = 1.f;
            int i = 0, j = 0, mat_id = 0;
            float inf = std::numeric_limits<float>::max();
            AABB root;
            root.p_min = {inf, inf, inf, 1.0};
            root.p_max = {-inf, -inf, -inf, 1.0};
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
                    cpu_tri_list.push_back(new TriangleCPU());
                    cpu_tri_list.back()->props.matID = mat_id;
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
                        cpu_tri_list.back()->props.v1 = {x,y,z,w};
                    else if(i == 2)
                        cpu_tri_list.back()->props.v2 = {x,y,z,w};
                    else if (i == 3)
                    {
                        cpu_tri_list.back()->props.v3 = {x,y,z,w};

                        //Compute AABB after 3rd vertex is fed.
                        cpu_tri_list.back()->computeCentroid();
                        i = 0;

                        for(int k = 0; k < 3; k++)
                        {
                            root.p_min.s[k] = std::min(root.p_min.s[k], cpu_tri_list.back()->aabb.p_min.s[k]);
                            root.p_max.s[k] = std::max(root.p_max.s[k], cpu_tri_list.back()->aabb.p_max.s[k]);
                        }
                    }
                }
                else if (extract == "n")
                {
                    j++;
                    w = 0.f;
                    ss >> x >> y >> z;

                    if(j == 1)
                        cpu_tri_list.back()->props.vn1 = {x,y,z,w};
                    else if(j == 2)
                        cpu_tri_list.back()->props.vn2 = {x,y,z,w};
                    else if (j == 3)
                    {
                        cpu_tri_list.back()->props.vn3 = {x,y,z,w};
                        j = 0;
                    }
                }
            }
            file.close();
            for(int i = 0; i < cpu_tri_list.size(); i++)
                vert_data.push_back(cpu_tri_list[i]->props);

            std::cout << "Total Triangles Loaded: " << vert_data.size() << std::endl;
            if(bvh.bins > 0)
            {
                std::cout << "Creating BVH..." << std::endl;
                bvh.createBVH(root, cpu_tri_list);
                std::cout << "BVH Size: " << bvh.gpu_node_list.size() << " Nodes" << std::endl;
            }
        }
        else
            throw RuntimeError("Error opening object file...");
    }

    void Scene::setBuffer()
    {
        int offset;
    }
}

