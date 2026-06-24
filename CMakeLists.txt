cmake_minimum_required(VERSION 3.20)
project(tinyrenderer-gui LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Dependencies via FetchContent ────────────────────────────────────────────
include(FetchContent)

# SDL2 — window management, input, texture display
FetchContent_Declare(
    sdl2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-2.30.9
    GIT_SHALLOW    TRUE
)
set(SDL_STATIC ON  CACHE BOOL "" FORCE)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_TEST   OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(sdl2)

# Dear ImGui — GUI overlay
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.4
    GIT_SHALLOW    TRUE
)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()

set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer2.cpp
)

# ── Our source files ─────────────────────────────────────────────────────────
set(RENDERER_SOURCES
    src/tgaimage.cpp
    src/model.cpp
    src/our_gl.cpp
    src/camera.cpp
    src/main.cpp
)

add_executable(tinyrenderer-gui ${RENDERER_SOURCES} ${IMGUI_SOURCES})

target_include_directories(tinyrenderer-gui PRIVATE
    src
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(tinyrenderer-gui PRIVATE SDL2::SDL2-static)

# OpenMP for parallel rasterization
find_package(OpenMP COMPONENTS CXX)
if(OpenMP_CXX_FOUND)
    target_link_libraries(tinyrenderer-gui PRIVATE OpenMP::OpenMP_CXX)
    message(STATUS "OpenMP found — parallel rasterization enabled")
else()
    message(STATUS "OpenMP not found — single-threaded rasterization")
endif()
