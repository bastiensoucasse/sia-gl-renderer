#pragma once

#include "trackball.h"

class Camera : public Trackball
{
public:
    Camera() {}

    static Eigen::Matrix4f perspective(float fovy, float aspect, float zNear, float zFar);
    static Eigen::Matrix4f lookAt(const Eigen::Vector3f& position, const Eigen::Vector3f& target, const Eigen::Vector3f& up);
};
