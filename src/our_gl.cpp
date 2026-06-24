#include <algorithm>
#include <cstdlib>
#include <cmath>
#include "our_gl.h"

mat<4,4> ModelView, Viewport, Perspective;
std::vector<double> zbuffer;

void lookat(const vec3 eye, const vec3 center, const vec3 up) {
    vec3 n = normalized(eye-center);
    vec3 l = normalized(cross(up,n));
    vec3 m = normalized(cross(n, l));
    ModelView = mat<4,4>{{{l.x,l.y,l.z,0}, {m.x,m.y,m.z,0}, {n.x,n.y,n.z,0}, {0,0,0,1}}} *
                mat<4,4>{{{1,0,0,-center.x}, {0,1,0,-center.y}, {0,0,1,-center.z}, {0,0,0,1}}};
}

void init_perspective(const double fov_radians, const double aspect,
                      const double, const double zfar) {
    double f = 1.0 / std::tan(fov_radians / 2.0);
    // FOV + aspect scaling for x,y (like standard OpenGL),
    // but w = -z_view/zfar + 1 avoids singularity at z_view=0
    // (unlike stock OpenGL's w=-z_view which requires the entire
    //  scene to be at negative z_view — not true when looking at origin)
    Perspective = {{{f/aspect, 0, 0, 0},
                    {0, f, 0, 0},
                    {0, 0, 1, 0},
                    {0, 0, -1/zfar, 1}}};
}

void init_viewport(const int x, const int y, const int w, const int h) {
    Viewport = {{{w/2., 0, 0, x+w/2.}, {0, -h/2., 0, y+h/2.}, {0,0,1,0}, {0,0,0,1}}};
}

void init_zbuffer(const int width, const int height) {
    zbuffer = std::vector(width*height, -1000.);
}

void rasterize(const Triangle &clip, const IShader &shader, TGAImage &framebuffer) {
    vec4 ndc[3]    = { clip[0]/clip[0].w, clip[1]/clip[1].w, clip[2]/clip[2].w };
    vec2 screen[3] = { (Viewport*ndc[0]).xy(), (Viewport*ndc[1]).xy(), (Viewport*ndc[2]).xy() };

    // Backface culling via signed area
    mat<3,3> ABC = {{ {screen[0].x, screen[0].y, 1.},
                      {screen[1].x, screen[1].y, 1.},
                      {screen[2].x, screen[2].y, 1.} }};
    mat<3,3> ABC_inv = ABC.invert_transpose();

    auto [bbminx,bbmaxx] = std::minmax({screen[0].x, screen[1].x, screen[2].x});
    auto [bbminy,bbmaxy] = std::minmax({screen[0].y, screen[1].y, screen[2].y});

    #pragma omp parallel for
    for (int x=std::max<int>(bbminx, 0); x<=std::min<int>(bbmaxx, framebuffer.width()-1); x++) {
        for (int y=std::max<int>(bbminy, 0); y<=std::min<int>(bbmaxy, framebuffer.height()-1); y++) {
            vec3 bc_screen = ABC_inv * vec3{static_cast<double>(x), static_cast<double>(y), 1.};

            if (bc_screen.x<0 || bc_screen.y<0 || bc_screen.z<0) continue;

            // Perspective-correct barycentric coordinates
            vec3 bc_clip = { bc_screen.x/clip[0].w, bc_screen.y/clip[1].w, bc_screen.z/clip[2].w };
            bc_clip = bc_clip / (bc_clip.x + bc_clip.y + bc_clip.z);

            double z = bc_screen * vec3{ ndc[0].z, ndc[1].z, ndc[2].z };
            if (z <= zbuffer[x+y*framebuffer.width()]) continue;

            auto [discard, color] = shader.fragment(bc_clip);
            if (discard) continue;

            if (color[3] < 254) {
                // Alpha blending: src_over
                TGAColor dst = framebuffer.get(x, y);
                double a = color[3] / 255.0;
                for (int ch : {0,1,2})
                    color[ch] = (uint8_t)(color[ch]*a + dst[ch]*(1.0-a));
                color[3] = 255;
                // Don't update z-buffer for transparent fragments
                framebuffer.set(x, y, color);
            } else {
                zbuffer[x+y*framebuffer.width()] = z;
                framebuffer.set(x, y, color);
            }
        }
    }
}

// Bresenham 2D line drawing
void draw_line(TGAImage &img, int x0, int y0, int x1, int y1, const TGAColor &color) {
    bool steep = false;
    if (std::abs(x0-x1) < std::abs(y0-y1)) {
        std::swap(x0, y0);
        std::swap(x1, y1);
        steep = true;
    }
    if (x0 > x1) {
        std::swap(x0, x1);
        std::swap(y0, y1);
    }
    int dx = x1 - x0;
    int dy = y1 - y0;
    int derror = std::abs(dy) * 2;
    int error = 0;
    int y = y0;
    for (int x = x0; x <= x1; x++) {
        if (steep)
            img.set(y, x, color);
        else
            img.set(x, y, color);
        error += derror;
        if (error > dx) {
            y += (y1 > y0 ? 1 : -1);
            error -= dx * 2;
        }
    }
}

void draw_triangle_wireframe(TGAImage &img, const vec2 pts[3], const TGAColor &color) {
    draw_line(img, pts[0].x, pts[0].y, pts[1].x, pts[1].y, color);
    draw_line(img, pts[1].x, pts[1].y, pts[2].x, pts[2].y, color);
    draw_line(img, pts[2].x, pts[2].y, pts[0].x, pts[0].y, color);
}
