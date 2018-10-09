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

#include <AreaLight.h>
#include <Eigen/Core>
#include <Eigen/Geometry>
namespace helion_render
{

    AreaLight::AreaLight()
    {
        //ctor
    }

    AreaLight::AreaLight(const Vec4f& pos, const Vec4f& diffuse_col, const Vec4f& specular_col, float intensity, const Vec4f& dir)
                    : pos(pos), diffuse_color(diffuse_col), specular_color(specular_col), intensity(intensity), dir(dir)
    {
        setEdgeVectors();
        //ctor
    }

    AreaLight::~AreaLight()
    {
        //dtor
    }

    void AreaLight::setEdgeVectors()
    {
        this->dir.normalize();
        Vec3f temp(pos.x() - 1, pos.y() - 1, pos.z() - 1), result;
        temp.normalize();

        result = dir.head<3>().cross(temp);
        edge_a << result.x(), result.y(), result.z(), 0.f;
        edge_a = edge_a.normalized() * 2.0f;

        result =  dir.head<3>().cross(edge_a.head<3>() );
        edge_b << result.x(), result.y(), result.z(), 0.f;
        edge_b = edge_b.normalized() * 8.0f;
    }

    void AreaLight::setEdgeVectors(const Vec4f& direction)
    {

        this->dir = direction.normalized();
        Vec3f temp(pos.x() - 1, pos.y() - 1, pos.z() - 1), result;
        temp.normalize();

        result = dir.head<3>().cross(temp);
        edge_a << result.x(), result.y(), result.z(), 0.f;
        edge_a = edge_a.normalized() * 2.0f;

        result =  dir.head<3>().cross(edge_a.head<3>() );
        edge_b << result.x(), result.y(), result.z(), 0.f;
        edge_b = edge_b.normalized() * 8.0f;
    }

    int AreaLight::setBuffer(std::vector<cl_float>& scene_data, int offset)
    {

    }
}
