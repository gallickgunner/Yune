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

#ifndef BVH_H
#define BVH_H

#include "CL_headers.h"
#include "TriangleCPU.h"
#include "BVHNodeCPU.h"
#include <vector>

namespace yune
{
    class BVH
    {
        public:
            BVH();
            BVH(int bins);
            ~BVH();
            std::vector<BVHNodeGPU> gpu_node_list;
            std::vector<cl_int> binary_heap;
            void createBVH(AABB root, std::vector<TriangleCPU*>& cpu_tri_list);
            int bins;
        private:
            enum SplitAxis
            {
                X = 0,
                Y = 1,
                Z = 2
            };

            std::vector<BVHNodeCPU> cpu_node_list;
            int leaf_primitives;
            float cost_isect, cost_trav;
            SplitAxis findSplitAxis(std::vector<TriangleCPU*>& cpu_tri_list, std::vector<int>& child_list);
            void populateChildNodes(const BVHNodeCPU& parent, BVHNodeCPU& c1, BVHNodeCPU& c2, std::vector<TriangleCPU*>& cpu_tri_list);
            void resizeBvh(std::vector<TriangleCPU*>& cpu_tri_list);
            void resizeBvhNode(BVHNodeCPU& node, std::vector<TriangleCPU*>& cpu_tri_list);
            AABB getExtent(const AABB& bb1, const AABB& bb2);
            float getSurfaceArea(AABB aabb);
    };
}

#endif // BVH_H
