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

#ifndef SCENE_H
#define SCENE_H
#include <CL_headers.h>

#include <Camera.h>

#include <vector>
#include <string>

namespace yune
{
    class Scene
    {
        public:
            Scene();
            Scene(Camera main_cam);
            ~Scene();
            void setBuffer( );
            void loadModel(std::string objfile, std::vector<Triangle>& scene_data, std::string matfile, std::vector<Material>& mat_data);

            Camera main_camera;


    };
}
#endif // SCENE_H
