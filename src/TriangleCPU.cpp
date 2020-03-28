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

#include "TriangleCPU.h"
#include <limits>
#include <algorithm>
namespace yune
{
    TriangleCPU::TriangleCPU()
    {
        //ctor
    }

    TriangleCPU::~TriangleCPU()
    {
        //dtor
    }

    void TriangleCPU::computeCentroid()
    {
        centroid = props.v1 + props.v2;
        centroid = centroid + props.v3;
        centroid = centroid/3.0f;
        centroid.s[3] = 1.0f;

        computeAABB();
    }

    void TriangleCPU::computeAABB()
    {
        cl_float4 p_min = {0,0,0,1};
        cl_float4 p_max = {0,0,0,1};

        for(int i = 0; i < 3; i++)
        {
            p_min.s[i] = std::min(props.v1.s[i], std::min(props.v2.s[i], props.v3.s[i]));
            p_max.s[i] = std::max(props.v1.s[i], std::max(props.v2.s[i], props.v3.s[i]));
        }
        cl_float4 diff = p_max - p_min;

        for(int i = 0; i < 3; i++)
        {
            if(diff.s[i] == 0.0f)
                p_max.s[i] += 0.2f;
        }

        aabb.p_min = p_min;
        aabb.p_max = p_max;
    }
}
