#pragma once
#include <string>
#include <vector>
#include "geometry.h"
#include "tgaimage.h"

class Model {
    std::vector<vec4> verts = {};
    std::vector<vec4> norms = {};
    std::vector<vec2> tex = {};
    std::vector<int> facet_vrt = {};
    std::vector<int> facet_nrm = {};
    std::vector<int> facet_tex = {};
    TGAImage diffusemap  = {};
    TGAImage normalmap   = {};
    TGAImage specularmap = {};
public:
    Model() = default;
    explicit Model(const std::string filename);
    bool load(const std::string filename);

    int nverts() const;
    int nfaces() const;
    vec4 vert(const int i) const;
    vec4 vert(const int iface, const int nthvert) const;
    vec4 normal(const int iface, const int nthvert) const;
    vec4 normal(const vec2 &uv) const;
    vec2 uv(const int iface, const int nthvert) const;
    const TGAImage& diffuse()  const;
    const TGAImage& specular() const;
    const TGAImage& normalmap_img() const;
    bool has_textures() const { return diffusemap.width() > 0; }
};
