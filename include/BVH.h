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
            ~BVH();
            std::vector<BVHNodeGPU> gpu_node_list;
            void createBVH(AABB root, const std::vector<TriangleCPU>& cpu_tri_list, int bvh_bins = 20);
            int bins;
            float bvh_size_kb, bvh_size_mb;

        private:
            enum SplitAxis
            {
                X = 0,
                Y = 1,
                Z = 2
            };

            SplitAxis findSplitAxis(const std::vector<TriangleCPU>& cpu_tri_list, std::vector<int>& child_list);
            AABB getExtent(const AABB& bb1, const AABB& bb2);

            void clearValues();
            void populateChildNodes(const BVHNodeCPU& parent, BVHNodeCPU& c1, BVHNodeCPU& c2, const std::vector<TriangleCPU>& cpu_tri_list);
            void resizeBvh(const std::vector<TriangleCPU>& cpu_tri_list);
            void resizeBvhNode(BVHNodeCPU& node, const std::vector<TriangleCPU>& cpu_tri_list);
            float getSurfaceArea(AABB aabb);

            std::vector<BVHNodeCPU> cpu_node_list;
            int leaf_primitives;
            float cost_isect, cost_trav;
    };
}

#endif // BVH_H
