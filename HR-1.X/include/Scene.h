/******************************************************************************
 *  This file is part of "Helion-Render".
 *
 *  Copyright (C) 2018 by Umair Ahmed and Syed Moiz Hussain.
 *
 *  "Helion-Render" is a Physically based Renderer using Bi-Directional Path Tracing.
 *  Right now the renderer only  works for devices that support OpenCL and OpenGL.
 *
 *  "Helion-Render" is a free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  "Helion-Render" is distributed in the hope that it will be useful,
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
#include <AreaLight.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>


#include <vector>
#include <string>

namespace helion_render
{
    class Scene
    {
        public:
            Scene();
            Scene(Camera main_cam, std::vector<AreaLight> light_sources);
            Scene(const Scene& scene);
            ~Scene();
            void setBuffer(std::vector<cl_float>& scene_data);
            void loadModel(std::string filename, bool recompute_normals, bool smooth_normals, float smooth_angle);

            Camera main_camera;
            std::vector<AreaLight> light_sources;
       // private:
            Assimp::Importer importer;
            const aiScene* scene;

    };
}
#endif // SCENE_H
