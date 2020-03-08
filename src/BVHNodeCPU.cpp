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

#include "BVHNodeCPU.h"

namespace yune
{
    BVHNodeCPU::BVHNodeCPU()
    {
        gpu_node.vert_len = -1;
        gpu_node.child_idx = -2;
        gpu_node.vert_list[0] = -1;
        gpu_node.vert_list[1] = -1;
        gpu_node.vert_list[2] = -1;
        gpu_node.vert_list[3] = -1;
        primitives.clear();
        //ctor
    }

    BVHNodeCPU::BVHNodeCPU(AABB aabb) : gpu_node{aabb, {-1,-1,-1,-1}, -1, -1}
    {
        gpu_node.vert_len = -1;
        gpu_node.child_idx = -2;
        gpu_node.vert_list[0] = -1;
        gpu_node.vert_list[1] = -1;
        gpu_node.vert_list[2] = -1;
        gpu_node.vert_list[3] = -1;
        primitives.clear();
        //ctor
    }
}
