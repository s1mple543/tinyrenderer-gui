# tinyrenderer-gui

A software 3D renderer with a real-time GUI, built from scratch in C++.

Based on [ssloy/tinyrenderer](https://github.com/ssloy/tinyrenderer) — a minimal OpenGL-like renderer implemented entirely in software (CPU rasterization). This fork adds an interactive GUI via SDL2 + Dear ImGui.

## Features

- **Software rasterization** — complete rendering pipeline: vertex shader → clipping → barycentric rasterization → fragment shader, all on CPU
- **5 shaders** — Phong (bump+specular mapping), flat, normal visualization, texture-only, depth
- **Perspective-correct interpolation** — proper attribute interpolation for UVs, normals across triangles
- **Tangent-space normal mapping** — TBN matrix construction per-fragment
- **Alpha blending** — RGBA texture transparency with src_over blending
- **Multi-model scene** — load and toggle any number of models simultaneously
- **Real-time orbit camera** — drag to rotate around scene center
- **Automatic model centering** — camera fits to the combined bounding box of visible models
- **Deferred loading** — models are loaded on first use, not at startup
- **Parallel rasterization** — OpenMP-accelerated pixel loops
- **OBJ + TGA** — Wavefront OBJ format (triangulated or quad) with TGA textures (diffuse, specular, normal map)

## Requirements

- C++20 compiler (MSVC 2022+)
- CMake ≥ 3.20
- Windows 10/11 (SDL2 + ImGui supported on Linux/macOS with minor changes)

Dependencies (SDL2, Dear ImGui) are fetched automatically by CMake.

## Quick Start

```bash
git clone <repo-url>
cd tinyrenderer-gui
cmake -B build -G Ninja -DCMAKE_CXX_COMPILER=cl -DCMAKE_C_COMPILER=cl
cmake --build build -j
./build/tinyrenderer-gui.exe
```

On Windows, you can also double-click `build.bat` from the project root.

Place your `.obj` files and `.tga` textures into the `obj/` directory. The app scans this directory at startup and lists all models in the GUI.

## Project Structure

```
tinyrenderer-gui/
├── CMakeLists.txt
├── build.bat
├── src/
│   ├── geometry.h      — template vector/matrix math library
│   ├── tgaimage.h/.cpp — TGA image loader
│   ├── model.h/.cpp    — OBJ model parser
│   ├── our_gl.h/.cpp   — rendering pipeline (MVP, rasterizer, z-buffer)
│   ├── camera.h/.cpp   — orbit camera
│   └── main.cpp        — shaders, GUI, main loop
├── obj/                — 3D models and textures
└── build/              — build output
```

## Controls

| Input | Action |
|-------|--------|
| Click & drag | Orbit camera |
| GUI checkboxes | Toggle model visibility |
| GUI sliders | Adjust FOV, lighting, shader |

## License

MIT
