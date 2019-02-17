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

#include <Camera.h>
#include <Eigen/Geometry>
#include <math.h>
#include <iostream>

namespace yune
{

    Camera::Camera() : y_FOV(60.0f), view_to_world_mat(), rotation_speed(0.25f), move_speed(0.4f)
    {
        //ctor
        setViewMatrix(Vec4f(1.f, 0.f, 0.f, 0.f),  // side
                      Vec4f(0.f, 1.f, 0.f, 0.f),  // up
                      Vec4f(0.f, 0.f, -1.f, 0.f), // look_at
                      Vec4f(0.f, 0.f, 0.f, 1.f)   // eye
                      );
        view_plane_dist =  1/tan(y_FOV*3.14/360);
        is_changed = true;
    }

    Camera::Camera(float y_FOV) : y_FOV(y_FOV), view_to_world_mat(), rotation_speed(0.25f), move_speed(0.4f)
   {
        setViewMatrix(Vec4f(1.f, 0.f, 0.f, 0.f),  // side
                      Vec4f(0.f, 1.f, 0.f, 0.f),  // up
                      Vec4f(0.f, 0.f, -1.f, 0.f), // look_at
                      Vec4f(0.f, 0.f, 0.f, 1.f)   // eye
                      );
        view_plane_dist =  1/tan(y_FOV*3.14/360);
        is_changed = true;

        //ctor
    }

    Camera::~Camera()
    {
        //dtor
    }

    void Camera::setBuffer(Cam* cam_data)
    {
        Vec4f temp = view_to_world_mat.row(0);
        cam_data->r1 = cl_float4{temp.x(), temp.y(), temp.z(), temp.w()};

        temp = view_to_world_mat.row(1);
        cam_data->r2 = cl_float4{temp.x(), temp.y(), temp.z(), temp.w()};

        temp = view_to_world_mat.row(2);
        cam_data->r3 = cl_float4{temp.x(), temp.y(), temp.z(), temp.w()};

        temp = view_to_world_mat.row(3);
        cam_data->r4 = cl_float4{temp.x(), temp.y(), temp.z(), temp.w()};

        cam_data->view_plane_dist = view_plane_dist;
        is_changed = false;
    }

    void Camera::setViewMatrix(const Vec4f& side, const Vec4f& up, const Vec4f& look_at, const Vec4f& eye)
    {
        this->side = side.normalized();
        this->up = up.normalized();
        this->look_at = look_at.normalized();
        this->eye = eye;
        view_to_world_mat << this->side, this->up, this->look_at * -1, this->eye;

    }

    void Camera::setOrientation(const Vec4f& dir, float pitch, float yaw)
    {

        Mat4x4f rotx = Mat4x4f::Identity();
        Mat4x4f roty = Mat4x4f::Identity();

        Eigen::AngleAxisf aaf_x( pitch * rotation_speed, Vec3f(this->side.x(), this->side.y(), this->side.z() ) );
        Eigen::AngleAxisf aaf_y( yaw * rotation_speed, Vec3f::UnitY());
        rotx.block<3,3>(0,0) = aaf_x.toRotationMatrix();
        roty.block<3,3>(0,0) = aaf_y.toRotationMatrix();

        Vec4f temp = rotx * this->up;

        // Rotate around local X axis in the range -90 to +90.
        if(temp.y() >= 0)
            view_to_world_mat = rotx * view_to_world_mat;
        view_to_world_mat = roty * view_to_world_mat;


        if(dir.norm() > 0)
        {
            if(dir.z() > 0)
                eye += look_at * move_speed;
            else if (dir.z() < 0)
                eye -= look_at * move_speed;
            else if (dir.x() > 0)
                eye += side * move_speed;
            else if (dir.x() < 0)
                eye -= side * move_speed;
            else if (dir.y() > 0)
                eye += up * move_speed;
            else if (dir.y() < 0)
                eye -= up * move_speed;
        }

        side = view_to_world_mat.col(0).normalized();
        up = view_to_world_mat.col(1).normalized();
        look_at = -view_to_world_mat.col(2).normalized();

        view_to_world_mat.col(3) = this->eye;
        is_changed = true;
    }

}
