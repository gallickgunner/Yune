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

#include "Camera.h"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/matrix_access.hpp"
#include "glm/vec3.hpp"

#include <cmath>

namespace yune
{

    Camera::Camera() : y_FOV(60.0f), view2world_mat(), rotation_speed(0.25f), move_speed(0.1f)
    {
        //ctor
        resetCamera();
    }

    Camera::Camera(float y_FOV, float rot_speed, float mov_speed) : y_FOV(y_FOV), view2world_mat(), rotation_speed(rot_speed), move_speed(mov_speed)
   {
        setViewMatrix(glm::vec4(1.f, 0.f, 0.f, 0.f),  // side
                      glm::vec4(0.f, 1.f, 0.f, 0.f),  // up
                      glm::vec4(0.f, 0.f, -1.f, 0.f), // look_at
                      glm::vec4(0.f, 0.f, 0.f, 1.f)   // eye
                      );
        view_plane_dist =  1/tan(y_FOV*3.14/360);
        is_changed = true;

        //ctor
    }

    Camera::~Camera()
    {
        //dtor
    }

    void Camera::updateViewPlaneDist()
    {
        view_plane_dist =  1/tan(y_FOV*3.14/360);
        is_changed = true;
    }

    void Camera::setBuffer(Cam* cam_data)
    {
        glm::vec4 temp = glm::row(view2world_mat, 0);
        cam_data->r1 = cl_float4{temp.x, temp.y, temp.z, temp.w};

        temp = glm::row(view2world_mat, 1);
        cam_data->r2 = cl_float4{temp.x, temp.y, temp.z, temp.w};

        temp = glm::row(view2world_mat, 2);
        cam_data->r3 = cl_float4{temp.x, temp.y, temp.z, temp.w};

        temp = glm::row(view2world_mat, 3);
        cam_data->r4 = cl_float4{temp.x, temp.y, temp.z, temp.w};

        cam_data->view_plane_dist = view_plane_dist;
        is_changed = false;
    }

    void Camera::updateViewMatrix()
    {
        side = glm::normalize(glm::column(view2world_mat, 0));
        up = glm::normalize(glm::column(view2world_mat, 1));
        look_at = -glm::normalize(glm::column(view2world_mat, 2));
        eye = glm::column(view2world_mat, 3);
        is_changed = true;
    }

    void Camera::resetCamera()
    {
        setViewMatrix(glm::vec4(1.f, 0.f, 0.f, 0.f),  // side
                      glm::vec4(0.f, 1.f, 0.f, 0.f),  // up
                      glm::vec4(0.f, 0.f, -1.f, 0.f), // look_at
                      glm::vec4(0.f, 0.f, 0.f, 1.f)   // eye
                      );
        y_FOV = 60;
        view_plane_dist =  1/tan(y_FOV*3.14/360);
        is_changed = true;
    }

    void Camera::setViewMatrix(const glm::vec4& side, const glm::vec4& up, const glm::vec4& look_at, const glm::vec4& eye)
    {
        this->eye = eye;
        this->side = glm::normalize(side);
        this->up = glm::normalize(up);
        this->look_at = glm::normalize(look_at);

        /* Always Remember that for Right Handed Coordinate Systems, the Camera (initially aligned with World Reference frame)
         * has the look direction negative to that of the Z axis. Hence to get the basis vector in Z we have to invert the look vector.
         */
        view2world_mat = glm::mat4(side, up, -look_at, eye);
        is_changed = true;
    }

    void Camera::setOrientation(const glm::vec4& dir, float pitch, float yaw)
    {
        if(dir.length() > 0)
        {
            if(dir.z > 0)
                eye += look_at * move_speed;
            else if (dir.z < 0)
                eye -= look_at * move_speed;
            else if (dir.x > 0)
                eye += side * move_speed;
            else if (dir.x < 0)
                eye -= side * move_speed;
            else if (dir.y > 0)
                eye += up * move_speed;
            else if (dir.y < 0)
                eye -= up * move_speed;

            if(pitch == 0 && yaw == 0)
            {
                view2world_mat = glm::column(view2world_mat, 3, eye);
                is_changed = true;
                return;
            }
        }

        glm::mat4x4 rotx, roty;
        rotx = glm::rotate(glm::mat4(1.0f), pitch * rotation_speed, glm::vec3(side.x, side.y, side.z) );
        roty = glm::rotate(glm::mat4(1.0f), yaw * rotation_speed, glm::vec3(0, 1, 0) );

        glm::vec4 temp = rotx * this->up;

        //Move to origin
        view2world_mat = glm::column(view2world_mat, 3, glm::vec4(0,0,0,1));

        // Rotate around local X axis in the range -90 to +90.
        if(temp.y >= 0)
            view2world_mat = rotx * view2world_mat;
        view2world_mat = roty * view2world_mat;

        //Move back to eye
        view2world_mat = glm::column(view2world_mat, 3, eye);

        side = glm::normalize(glm::column(view2world_mat, 0));
        up = glm::normalize(glm::column(view2world_mat, 1));
        look_at = -glm::normalize(glm::column(view2world_mat, 2));

        is_changed = true;
    }

}
