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

#ifndef AREALIGHT_H
#define AREALIGHT_H

#include <CL_headers.h>
#include <Eigen_typedefs.h>

#include <vector>

namespace helion_render
{
    /** \brief This class defines an Area light and it's properties.
     *
     *  This class contains the diffuse and specular color of the light, it's  intensity and the shape by storing 2 edge vectors and a direction.
     *  The shape of the Area Light is a Rectangle.
     */
    class AreaLight
    {
        public:
            AreaLight();    /**< Default Constructor. */

            /** \brief Overloaded Constructor.
             *
             * \param[in] pos A vec4 determining the position of the Light.
             * \param[in] diffuse_col A vec4 determining the diffuse color of the Light.
             * \param[in] specular_col A vec4 determining the specular color of the Light.
             * \param[in] intensity The intensity of the light.
             * \param[in] dir A vec4 determining the direction where the Light is facing. This can be used as the normal to the plane
             *            of the rectangle. This helps in calculating the edge vectors. If not provided automatically calculated when
             *            using acceleration data structures to point towards the center of the Root Bounding Box.
             *
             */
            AreaLight(const Vec4f& pos, const Vec4f& diffuse_col, const Vec4f& specular_col, float intensity, const Vec4f& dir = Vec4f(0,0,0,0));

            ~AreaLight();   /**< Default Destructor. */

            /** \brief Pass in values to the Light Buffer.
             *
             * \param[in,out] scene_data The Renderer::scene_data array passed in which Light data is also stored.
             * \param[in] offset The offset from which to start writing Light data in the Renderer::scene_data array.
             * \return Returns the offset/position at which the Light data has finished writing values.
             *
             */
            int setBuffer(std::vector<cl_float>& scene_data, int offset);

            /** \brief Compute the Edge vectors determining the light's rectangle.
             *
             * \param[in] dir The direction the Area light is facing.
             */
            void setEdgeVectors(const Vec4f& direction);

            Vec4f pos;              /**<  A vec4 determining the position of the Light. */
            Vec4f edge_a;           /**<  A vec4 determining the First Edge of the Rectangle. */
            Vec4f edge_b;           /**<  A vec4 determining the Second Edge of the Rectangle. */
            Vec4f dir;              /**<  A vec4 determining the direction the Area light is facing towards. */
            Vec4f diffuse_color;    /**<  A vec4 determining the diffuse color of the Light. */
            Vec4f specular_color;   /**<  A vec4 determining the specular color of the Light. */
            float intensity;        /**<  Intensity of the Light source. */

        private:
            void setEdgeVectors();

    };
}
#endif // AREALIGHT_H
