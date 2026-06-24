#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer2.h>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <numbers>

#include "geometry.h"
#include "tgaimage.h"
#include "model.h"
#include "our_gl.h"
#include "camera.h"

namespace fs = std::filesystem;

// ══════════════════════════════════════════════════════════════════════════════
//  Settings
// ══════════════════════════════════════════════════════════════════════════════

enum ShaderType {
    SHADER_PHONG,
    SHADER_FLAT,
    SHADER_NORMAL_VIS,
    SHADER_TEXTURE_ONLY,
    SHADER_DEPTH,
    SHADER_COUNT
};
static const char* kShaderNames[] = { "Phong (Bump+Spec)", "Flat", "Normal Visualize",
                                      "Texture Only", "Depth" };

struct Settings {
    ShaderType shader_type = SHADER_PHONG;
    float light_dir[3]     = { 1, 1, 1 };
    float ambient          = 0.4f;
    float fov_degrees      = 90.0f;
    float z_near           = 0.1f;
    float z_far            = 100.0f;
    bool  show_wireframe   = false;
    bool  enable_textures  = true;
    bool  enable_normalmap = true;
    int   fb_width         = 800;
    int   fb_height        = 800;
};

struct AppState {
    Camera        camera;
    Settings      settings;
    std::vector<Model>  models;          // all models (loaded on demand)
    std::vector<bool>   model_loaded;    // true once load() was called
    std::vector<bool>   model_visible;   // toggle per model
    bool          render_dirty   = true;
    std::vector<std::string> model_paths;
    std::vector<std::string> model_names;

    // Performance
    float fps         = 60;
    float render_ms   = 0;
    int   tri_count   = 0;
};

// ══════════════════════════════════════════════════════════════════════════════
//  Helpers
// ══════════════════════════════════════════════════════════════════════════════

static std::vector<std::string> find_obj_files(const std::string &root) {
    std::vector<std::string> out;
    if (!fs::exists(root)) return out;
    for (auto &e : fs::recursive_directory_iterator(root))
        if (e.path().extension() == ".obj")
            out.push_back(e.path().generic_string());
    return out;
}

static inline vec4 vertex_transform(const Model &m, int face, int vert) {
    return Perspective * ModelView * m.vert(face, vert);
}

static void fit_camera_to_visible(AppState &state) {
    Camera &cam = state.camera;
    double minx=1e9, miny=1e9, minz=1e9;
    double maxx=-1e9, maxy=-1e9, maxz=-1e9;
    bool any = false;
    for (size_t mi = 0; mi < state.models.size(); mi++) {
        if (!state.model_visible[mi] || !state.model_loaded[mi]) continue;
        any = true;
        const Model &m = state.models[mi];
        for (int i = 0; i < m.nverts(); i++) {
            vec4 v = m.vert(i);
            if (v.x < minx) minx=v.x; if (v.x > maxx) maxx=v.x;
            if (v.y < miny) miny=v.y; if (v.y > maxy) maxy=v.y;
            if (v.z < minz) minz=v.z; if (v.z > maxz) maxz=v.z;
        }
    }
    if (!any) return;
    cam.center = {(minx+maxx)/2, (miny+maxy)/2, (minz+maxz)/2};
    double sz = std::max({maxx-minx, maxy-miny, maxz-minz});
    cam.dist  = sz * 1.5;
    if (cam.dist < 0.5) cam.dist = 2.5;
    cam.yaw  = -0.4636;
    cam.pitch = 0.0;
}

// ══════════════════════════════════════════════════════════════════════════════
//  Shaders
// ══════════════════════════════════════════════════════════════════════════════

struct PhongShader : IShader {
    const Model &model;
    Settings     settings;
    vec4         light_eye;
    vec2  varying_uv[3];
    vec4  varying_nrm[3];
    vec4  tri[3];

    PhongShader(const Model &m, const Settings &s, const vec3 light_world)
        : model(m), settings(s) {
        light_eye = normalized(ModelView * vec4{light_world.x, light_world.y, light_world.z, 0});
    }

    vec4 vertex(int face, int vert) override {
        varying_uv[vert]  = model.uv(face, vert);
        varying_nrm[vert] = ModelView.invert_transpose() * model.normal(face, vert);
        vec4 gl_Position  = ModelView * model.vert(face, vert);
        tri[vert]         = gl_Position;
        return Perspective * gl_Position;
    }

    std::pair<bool,TGAColor> fragment(const vec3 bar) const override {
        double amb = settings.ambient;
        vec4 n = normalized(varying_nrm[0]*bar[0] + varying_nrm[1]*bar[1] + varying_nrm[2]*bar[2]);

        if (settings.enable_normalmap && model.normalmap_img().width() > 0) {
            mat<2,4> E = { tri[1]-tri[0], tri[2]-tri[0] };
            mat<2,2> U = { varying_uv[1]-varying_uv[0], varying_uv[2]-varying_uv[0] };
            mat<2,4> T = U.invert() * E;
            vec4 t = normalized(T[0]);
            vec4 b = normalized(T[1]);
            vec2 uv = varying_uv[0]*bar[0] + varying_uv[1]*bar[1] + varying_uv[2]*bar[2];
            mat<4,4> Dframe = mat<4,4>{{{t.x,t.y,t.z,0},{b.x,b.y,b.z,0},{n.x,n.y,n.z,0},{0,0,0,1}}};
            n = normalized(Dframe.transpose() * model.normal(uv));
        }

        double diffuse  = std::max(0., n * light_eye);
        vec4 r = normalized(n * (n * light_eye)*2 - light_eye);
        double specular = 0;

        vec2 uv = varying_uv[0]*bar[0] + varying_uv[1]*bar[1] + varying_uv[2]*bar[2];
        TGAColor tex_color = {255,255,255,255};
        if (settings.enable_textures && model.diffuse().width() > 0) {
            tex_color = sample2D(model.diffuse(), uv);
            if (model.diffuse().bytespp() < 4) tex_color[3] = 255; // no alpha
            if (model.specular().width() > 0) {
                double gloss = 0.5 + 2.0 * sample2D(model.specular(), uv)[0] / 255.0;
                specular = gloss * std::pow(std::max(r.z, 0.), 35);
            }
        }

        TGAColor c = tex_color;
        for (int ch : {0,1,2})
            c[ch] = std::min<int>(255, c[ch] * (amb + diffuse + specular));
        return {false, c};
    }
};

struct FlatShader : IShader {
    const Model &model;
    FlatShader(const Model &m) : model(m) {}
    vec4 vertex(int face, int vert) override {
        return Perspective * ModelView * model.vert(face, vert);
    }
    std::pair<bool,TGAColor> fragment(const vec3) const override {
        return {false, TGAColor{180, 180, 220, 255}};
    }
};

struct NormalVisShader : IShader {
    const Model &model;
    NormalVisShader(const Model &m) : model(m) {}
    vec4 vertex(int face, int vert) override {
        return Perspective * ModelView * model.vert(face, vert);
    }
    std::pair<bool,TGAColor> fragment(const vec3) const override {
        return {false, TGAColor{128, 128, 255, 255}};
    }
};

struct TextureOnlyShader : IShader {
    const Model &model;
    Settings     settings;
    vec2 varying_uv[3];
    TextureOnlyShader(const Model &m, const Settings &s) : model(m), settings(s) {}
    vec4 vertex(int face, int vert) override {
        varying_uv[vert] = model.uv(face, vert);
        return Perspective * ModelView * model.vert(face, vert);
    }
    std::pair<bool,TGAColor> fragment(const vec3 bar) const override {
        vec2 uv = varying_uv[0]*bar[0] + varying_uv[1]*bar[1] + varying_uv[2]*bar[2];
        TGAColor c = {200, 200, 200, 255};
        if (settings.enable_textures && model.diffuse().width() > 0) {
            c = sample2D(model.diffuse(), uv);
            if (model.diffuse().bytespp() < 4) c[3] = 255;
        }
        return {false, c};
    }
};

struct DepthShader : IShader {
    const Model &model;
    DepthShader(const Model &m) : model(m) {}
    vec4 vertex(int face, int vert) override {
        return Perspective * ModelView * model.vert(face, vert);
    }
    std::pair<bool,TGAColor> fragment(const vec3) const override {
        return {false, TGAColor{128, 128, 128, 255}};
    }
};

// ══════════════════════════════════════════════════════════════════════════════
//  Rendering (multi-model)
// ══════════════════════════════════════════════════════════════════════════════

static void render_scene(TGAImage &fb, AppState &state) {
    const Settings &settings = state.settings;
    vec3 eye = state.camera.eye();
    lookat(eye, state.camera.center, vec3{0,1,0});
    double aspect = static_cast<double>(fb.width()) / fb.height();
    init_perspective(settings.fov_degrees * std::numbers::pi / 180.0,
                     aspect, settings.z_near, settings.z_far);
    init_viewport(0, 0, fb.width(), fb.height());
    init_zbuffer(fb.width(), fb.height());

    // Clear framebuffer
    TGAColor bg{177, 195, 209, 255};
    for (int y = 0; y < fb.height(); y++)
        for (int x = 0; x < fb.width(); x++)
            fb.set(x, y, bg);

    vec3 light{ settings.light_dir[0], settings.light_dir[1], settings.light_dir[2] };
    int total_tris = 0;

    for (size_t mi = 0; mi < state.models.size(); mi++) {
        if (!state.model_visible[mi] || !state.model_loaded[mi]) continue;
        const Model &model = state.models[mi];

        IShader *shader = nullptr;
        PhongShader    phong_shader(model, settings, light);
        FlatShader     flat_shader(model);
        NormalVisShader nvis_shader(model);
        TextureOnlyShader tex_shader(model, settings);
        DepthShader    depth_shader(model);

        switch (settings.shader_type) {
            case SHADER_PHONG:       shader = &phong_shader;   break;
            case SHADER_FLAT:        shader = &flat_shader;    break;
            case SHADER_NORMAL_VIS:  shader = &nvis_shader;    break;
            case SHADER_TEXTURE_ONLY:shader = &tex_shader;     break;
            case SHADER_DEPTH:       shader = &depth_shader;   break;
            default:                 shader = &phong_shader;   break;
        }

        for (int f = 0; f < model.nfaces(); f++) {
            Triangle clip = { shader->vertex(f, 0),
                              shader->vertex(f, 1),
                              shader->vertex(f, 2) };
            rasterize(clip, *shader, fb);
        }
        total_tris += model.nfaces();

        if (settings.show_wireframe) {
            TGAColor wire_col{0, 0, 0, 255};
            for (int f = 0; f < model.nfaces(); f++) {
                vec2 screen[3];
                for (int v = 0; v < 3; v++) {
                    vec4 clip = vertex_transform(model, f, v);
                    vec4 ndc  = clip / clip.w;
                    screen[v] = (Viewport * ndc).xy();
                }
                draw_triangle_wireframe(fb, screen, wire_col);
            }
        }
    }
    state.tri_count = total_tris;
}

// ══════════════════════════════════════════════════════════════════════════════
//  GUI
// ══════════════════════════════════════════════════════════════════════════════

static void build_gui(AppState &state, int fb_w, int fb_h) {
    if (ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::SeparatorText("Models");
        // Toggle all
        bool any_on = false, any_off = false;
        for (bool v : state.model_visible) { if (v) any_on=true; else any_off=true; }
        if (ImGui::Button("All On")) {
            for (size_t i=0; i<state.model_visible.size(); i++) {
                state.model_visible[i]=true;
                if (!state.model_loaded[i]) {
                    state.models[i].load(state.model_paths[i]);
                    state.model_loaded[i] = true;
                }
            }
            fit_camera_to_visible(state);
            state.render_dirty=true;
        }
        ImGui::SameLine();
        if (ImGui::Button("All Off")) {
            for (size_t i=0; i<state.model_visible.size(); i++) state.model_visible[i]=false;
            state.render_dirty=true;
        }
        // Per-model checkboxes
        ImGui::BeginChild("model_list", ImVec2(0, std::min(200.0f, (float)state.model_names.size()*24)), true);
        for (size_t i = 0; i < state.model_names.size(); i++) {
            bool vis = state.model_visible[i];
            if (ImGui::Checkbox(state.model_names[i].c_str(), &vis)) {
                if (vis && !state.model_loaded[i]) {
                    // Load on first check
                    state.models[i].load(state.model_paths[i]);
                    state.model_loaded[i] = true;
                }
                state.model_visible[i] = vis;
                fit_camera_to_visible(state);
                state.render_dirty = true;
            }
        }
        ImGui::EndChild();

        ImGui::Separator();
        // Shader
        int shader_i = (int)state.settings.shader_type;
        if (ImGui::BeginCombo("Shader", kShaderNames[shader_i])) {
            for (int i = 0; i < SHADER_COUNT; i++) {
                if (ImGui::Selectable(kShaderNames[i], i == shader_i)) {
                    state.settings.shader_type = (ShaderType)i;
                    state.render_dirty = true;
                }
                if (i == shader_i) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::SeparatorText("Lighting");
        if (ImGui::SliderFloat3("Light Dir", state.settings.light_dir, -1, 1))
            state.render_dirty = true;
        if (ImGui::SliderFloat("Ambient", &state.settings.ambient, 0, 1, "%.2f"))
            state.render_dirty = true;

        ImGui::SeparatorText("Camera");
        if (ImGui::SliderFloat("FOV", &state.settings.fov_degrees, 10, 120, "%.0f°"))
            state.render_dirty = true;

        ImGui::SeparatorText("Display");
        if (ImGui::Checkbox("Textures",  &state.settings.enable_textures))
            state.render_dirty = true;
        if (ImGui::Checkbox("Normal Map",&state.settings.enable_normalmap))
            state.render_dirty = true;
        if (ImGui::Checkbox("Wireframe", &state.settings.show_wireframe))
            state.render_dirty = true;

        ImGui::Separator();
        if (ImGui::Button("Reset Camera")) {
            fit_camera_to_visible(state);
            state.render_dirty = true;
        }

        ImGui::SeparatorText("Performance");
        ImGui::Text("FPS:        %.1f", state.fps);
        ImGui::Text("Triangles:  %d",   state.tri_count);
        ImGui::Text("Render:     %.2f ms", state.render_ms);
    }
    ImGui::End();
}

// ══════════════════════════════════════════════════════════════════════════════
//  Entry point
// ══════════════════════════════════════════════════════════════════════════════

int main(int argc, char *argv[]) {
    // ── Find models ──────────────────────────────────────────────────────
    std::string obj_dir = "./obj";
    if (argc > 1) obj_dir = argv[1];

    auto obj_files = find_obj_files(obj_dir);
    if (obj_files.empty()) { obj_dir = "../obj"; obj_files = find_obj_files(obj_dir); }
    if (obj_files.empty()) {
        std::cerr << "No .obj files found.  Pass path as argument:\n  tinyrenderer-gui path/to/obj\n";
    }

    // ── Init SDL2 ─────────────────────────────────────────────────────────
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL_Init: " << SDL_GetError() << "\n";
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("tinyrenderer-gui",
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          1280, 960, SDL_WINDOW_RESIZABLE);
    if (!window) { std::cerr << "SDL_CreateWindow: " << SDL_GetError() << "\n"; return 1; }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1,
            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) { std::cerr << "SDL_CreateRenderer: " << SDL_GetError() << "\n"; return 1; }

    // ── Framebuffer ───────────────────────────────────────────────────────
    AppState state;
    int fb_w = state.settings.fb_width;
    int fb_h = state.settings.fb_height;

    TGAImage framebuffer(fb_w, fb_h, TGAImage::RGBA);
    SDL_Texture *fb_tex = SDL_CreateTexture(renderer,
            SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING, fb_w, fb_h);

    // ── Init ImGui ────────────────────────────────────────────────────────
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

    // ── Register all models (deferred loading) ─────────────────────────
    for (auto &p : obj_files) {
        state.model_paths.push_back(p);
        auto stem   = fs::path(p).stem().string();
        auto parent = fs::path(p).parent_path().filename().string();
        state.model_names.push_back(
            (!parent.empty() && parent != "obj" && parent != stem)
                ? parent + "/" + stem : stem);
        state.models.emplace_back();           // empty model
        state.model_loaded.push_back(false);   // not loaded yet
        state.model_visible.push_back(false);  // initially hidden
    }
    state.render_dirty = false; // nothing to render yet

    // ── Main loop ─────────────────────────────────────────────────────────
    using Clock = std::chrono::steady_clock;
    auto last_time  = Clock::now();
    int  frame_count = 0;
    float fps_timer  = 0;

    bool running = true;
    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            ImGui_ImplSDL2_ProcessEvent(&ev);
            if (ev.type == SDL_QUIT) running = false;

            if (ev.type == SDL_MOUSEMOTION && (ev.motion.state & SDL_BUTTON_LMASK)) {
                if (!ImGui::GetIO().WantCaptureMouse) {
                    state.camera.orbit(ev.motion.xrel * 0.006, ev.motion.yrel * 0.006);
                    state.render_dirty = true;
                }
            }
        }

        // ── Render scene (all visible models) ──
        if (state.render_dirty) {
            auto t0 = Clock::now();
            render_scene(framebuffer, state);
            state.render_ms = std::chrono::duration<float, std::milli>(
                Clock::now() - t0).count();
            state.render_dirty = false;
        }

        // ── Upload + ImGui ────────────────────────────────────────────────
        SDL_UpdateTexture(fb_tex, nullptr, framebuffer.pixels(), fb_w * 4);

        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        build_gui(state, fb_w, fb_h);
        ImGui::Render();

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, fb_tex, nullptr, nullptr);
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);

        // ── FPS ───────────────────────────────────────────────────────────
        frame_count++;
        auto now = Clock::now();
        float dt = std::chrono::duration<float>(now - last_time).count();
        fps_timer += dt;
        if (fps_timer >= 0.5f) {
            state.fps = frame_count / fps_timer;
            frame_count = 0;
            fps_timer = 0;
        }
        last_time = now;
    }

    // ── Cleanup ───────────────────────────────────────────────────────────
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyTexture(fb_tex);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
