/******************************************************************************
 *  This file is part of "Yune".
 *
 *  Copyright (C) 2018 by Umair Ahmed and Syed Moiz Hussain.
 *
 *  "Yune" is a Physically based Renderer using Bi-Directional Path Tracing.
 *  Right now the renderer only  works for devices that support OpenCL and OpenGL.
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
namespace yune
{
    Scene::Scene()
    {
        //ctor
    }

    Scene::Scene(Camera cam, std::vector<AreaLight> lightsources) : main_camera(cam), light_sources(lightsources)
    {

    }

    Scene::Scene(const Scene& scene) : main_camera(scene.main_camera)
    {
        //Copy only Light sources and Camera.
        for(int i = 0; i < scene.light_sources.size(); i++)
            this->light_sources.push_back(AreaLight(scene.light_sources[i]));

    }

    Scene::~Scene()
    {

    }

    void Scene::loadModel(std::string filename, bool recompute_normals, bool smooth_normals, float smooth_angle)
    {
        int remove_components;
        unsigned int flags;
        remove_components = aiComponent_TANGENTS_AND_BITANGENTS |
                            aiComponent_ANIMATIONS |
                            aiComponent_BONEWEIGHTS |
                            aiComponent_CAMERAS |
                            aiComponent_TEXCOORDS |
                            aiComponent_TEXTURES;

        flags = aiProcess_Triangulate |
                aiProcess_SortByPType |
                aiProcess_RemoveComponent |
                //aiProcess_PreTransformVertices;
                aiProcess_OptimizeMeshes |
                aiProcess_OptimizeGraph;

        if(recompute_normals)
        {
            remove_components = remove_components | aiComponent_NORMALS;
            if(smooth_normals)
            {
                importer.SetPropertyFloat(AI_CONFIG_PP_GSN_MAX_SMOOTHING_ANGLE, smooth_angle);
                flags = flags | aiProcess_GenSmoothNormals;
            }
            else
                flags = flags | aiProcess_GenNormals;
        }

        importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_LINE | aiPrimitiveType_POINT);
        importer.SetPropertyInteger(AI_CONFIG_PP_RVC_FLAGS, remove_components);

        if(!importer.ValidateFlags(flags))
            throw RuntimeError("Error validating Flags..");

        scene = importer.ReadFile(filename, flags);

        if( !scene)
        {
            std::string err = "\nError Loading Model file.\n";
            err += importer.GetErrorString();
            throw RuntimeError(err);
        }

    }

    void Scene::setBuffer(std::vector<cl_float>& scene_data)
    {
        int offset;
        for(int i = 0; i < light_sources.size(); i++)
        {
            offset = light_sources[i].setBuffer(scene_data, offset);
        }

    }
}

