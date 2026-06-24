#include <fstream>
#include <sstream>
#include <iostream>
#include "model.h"

Model::Model(const std::string filename) {
    load(filename);
}

bool Model::load(const std::string filename) {
    verts.clear(); norms.clear(); tex.clear();
    facet_vrt.clear(); facet_nrm.clear(); facet_tex.clear();
    diffusemap = {}; normalmap = {}; specularmap = {};

    std::ifstream in;
    in.open(filename, std::ifstream::in);
    if (in.fail()) {
        std::cerr << "Failed to open " << filename << std::endl;
        return false;
    }
    std::string line;
    while (!in.eof()) {
        std::getline(in, line);
        std::istringstream iss(line.c_str());
        char trash;
        if (!line.compare(0, 2, "v ")) {
            iss >> trash;
            vec4 v = {0,0,0,1};
            for (int i : {0,1,2}) iss >> v[i];
            verts.push_back(v);
        } else if (!line.compare(0, 3, "vn ")) {
            iss >> trash >> trash;
            vec4 n;
            for (int i : {0,1,2}) iss >> n[i];
            norms.push_back(normalized(n));
        } else if (!line.compare(0, 3, "vt ")) {
            iss >> trash >> trash;
            vec2 uv;
            for (int i : {0,1}) iss >> uv[i];
            tex.push_back({uv.x, 1-uv.y});
        } else if (!line.compare(0, 2, "f ")) {
            // Buffer all vertex indices for this face
            struct Triplet { int v, t, n; };
            std::vector<Triplet> idx;
            iss >> trash; // eat 'f'
            int a;
            char s1, s2;
            while (iss >> a) {
                Triplet tri = {a-1, -1, -1};
                if (iss.peek() == '/') {
                    iss >> s1;
                    if (iss.peek() != '/') { iss >> tri.t; tri.t--; }
                    if (iss.peek() == '/') { iss >> s2 >> tri.n; tri.n--; }
                }
                idx.push_back(tri);
            }
            // Triangulate: fan from vertex 0
            for (size_t i = 1; i+1 < idx.size(); i++) {
                int order[3] = {0, (int)i, (int)i+1};
                for (int k : order) {
                    facet_vrt.push_back(idx[k].v);
                    facet_tex.push_back(std::max(0, idx[k].t));
                    facet_nrm.push_back(std::max(0, idx[k].n));
                }
            }
        }
    }
    // Ensure dummy entries so facet indices never go out of bounds
    if (facet_vrt.size()>0 && norms.size()==0) norms.push_back({0,0,1,0});
    if (facet_vrt.size()>0 && tex.size()==0)   tex.push_back({0,0});

    std::cerr << "# v# " << nverts() << " f# "  << nfaces() << std::endl;

    auto load_texture = [&filename](const std::string suffix, TGAImage &img) {
        size_t dot = filename.find_last_of(".");
        if (dot==std::string::npos) return;
        std::string texfile = filename.substr(0,dot) + suffix;
        std::cerr << "texture file " << texfile << " loading "
                  << (img.read_tga_file(texfile.c_str()) ? "ok" : "failed") << std::endl;
    };
    load_texture("_diffuse.tga",    diffusemap);
    load_texture("_nm_tangent.tga", normalmap);
    load_texture("_spec.tga",       specularmap);

    return true;
}

int Model::nverts() const { return verts.size(); }
int Model::nfaces() const { return facet_vrt.size()/3; }

vec4 Model::vert(const int i) const {
    return verts[i];
}

vec4 Model::vert(const int iface, const int nthvert) const {
    return verts[facet_vrt[iface*3+nthvert]];
}

vec4 Model::normal(const int iface, const int nthvert) const {
    return norms[facet_nrm[iface*3+nthvert]];
}

vec4 Model::normal(const vec2 &uv) const {
    if (normalmap.width() == 0) return {0,0,1,0};
    TGAColor c = normalmap.get(uv[0]*normalmap.width(), uv[1]*normalmap.height());
    return normalized(vec4{(double)c[2],(double)c[1],(double)c[0],0}*2./255. - vec4{1,1,1,0});
}

vec2 Model::uv(const int iface, const int nthvert) const {
    return tex[facet_tex[iface*3+nthvert]];
}

const TGAImage& Model::diffuse()  const { return diffusemap;  }
const TGAImage& Model::specular() const { return specularmap; }
const TGAImage& Model::normalmap_img() const { return normalmap; }
