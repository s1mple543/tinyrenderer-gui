#include "camera.h"
#include <cmath>
#include <algorithm>

vec3 Camera::eye() const {
    return center + vec3{
        dist * std::cos(pitch) * std::sin(yaw),
        dist * std::sin(pitch),
        dist * std::cos(pitch) * std::cos(yaw)
    };
}

void Camera::orbit(double dyaw, double dpitch) {
    yaw   += dyaw;
    pitch += dpitch;
    pitch = std::max(-1.5, std::min(1.5, pitch));
}

void Camera::pan(double dx, double dy) {
    double rx =  std::cos(yaw);
    double rz = -std::sin(yaw);
    double fx =  std::sin(yaw);
    double fz =  std::cos(yaw);
    center.x += rx * dx + fx * dy;
    center.z += rz * dx + fz * dy;
}
