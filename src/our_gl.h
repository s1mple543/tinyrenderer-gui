#pragma once
#include "geometry.h"
#include "tgaimage.h"

// OpenGL-style state matrices
extern mat<4,4> ModelView;
extern mat<4,4> Viewport;
extern mat<4,4> Perspective;

// Z-buffer
extern std::vector<double> zbuffer;

// Pipeline functions
void lookat(const vec3 eye, const vec3 center, const vec3 up);
void init_perspective(const double fov_radians, const double aspect, const double znear, const double zfar);
void init_viewport(const int x, const int y, const int w, const int h);
void init_zbuffer(const int width, const int height);

// Shader interface
struct IShader {
    static TGAColor sample2D(const TGAImage &img, const vec2 &uvf) {
        return img.get(uvf[0] * img.width(), uvf[1] * img.height());
    }
    virtual vec4 vertex(const int face, const int vert) = 0;
    virtual std::pair<bool,TGAColor> fragment(const vec3 bar) const = 0;
    virtual ~IShader() = default;
};

typedef vec4 Triangle[3];
void rasterize(const Triangle &clip, const IShader &shader, TGAImage &framebuffer);

// Utility: draw a line on the framebuffer (Bresenham)
void draw_line(TGAImage &img, int x0, int y0, int x1, int y1, const TGAColor &color);

// Utility: draw triangle edges as wireframe
void draw_triangle_wireframe(TGAImage &img, const vec2 pts[3], const TGAColor &color);
