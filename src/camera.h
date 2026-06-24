#pragma once
#include "geometry.h"

struct Camera {
    vec3 center = {0, 0, 0};
    double yaw    = -0.4636;
    double pitch  = 0.0;
    double dist   = 2.236;

    vec3 eye() const;
    void orbit(double dyaw, double dpitch);
    void pan(double dx, double dy);
};
