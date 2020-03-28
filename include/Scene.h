/******************************************************************************
 *  This file is part of Yune".
 *
 *  Copyright (C) 2018 by Umair Ahmed and Syed Moiz Hussain.
 *
 *  "Yune" is a personal project and an educational raytracer/pathtracer. It's aimed at young
 *  researchers trying to implement raytracing/pathtracing but don't want to mess with boilerplate code.
 *  It provides all basic functionality to open a window, save images, etc. Since it's GPU based, you only need to modify
 *  the kernel file and see your program in action. For details, check <https://github.com/gallickgunner/Yune>
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

#ifndef SCENE_H
#define SCENE_H

#include "CL_headers.h"
#include "Camera.h"
#include "TriangleCPU.h"
#include "BVH.h"

#include <vector>
#include <string>

namespace yune
{
    class Scene
    {
        public:
            Scene();
            ~Scene();
            void setBuffer( );
            void loadModel(std::string filepath, std::string filename);
            void loadBVH(int bvh_bins);
            void reloadMatFile();

            Camera main_camera;
            std::vector<TriangleGPU> vert_data;
            std::vector<Material> mat_data;
            std::string scene_file, mat_file, mat_filename;
            AABB root;
            BVH bvh;
            int num_triangles;
            float scene_size_kb, scene_size_mb;

        private:
            void clearValues();
            std::string getMatFileName(std::string filepath);
            std::vector<TriangleCPU> cpu_tri_list;
    };
}
#endif // SCENE_H
