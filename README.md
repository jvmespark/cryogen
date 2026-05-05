# CryoGen

Vulkan game engine featuring real-time
physically-based rendering, hardware ray tracing (`VK_KHR_ray_tracing_pipeline`),
an impulse-based rigid-body solver, an authoritative-server multiplayer stack
built on Boost.Asio, an ImGui-driven level editor, and Assimp-backed mesh
loading.

```
                       ┌──────────────────────────────┐
                       │        apps/editor           │  ImGui level editor
                       │        apps/runtime          │  Standalone client
                       │        apps/server           │  Headless server
                       └────────────────┬─────────────┘
                                        │
                       ┌────────────────┴─────────────┐
                       │      cryogen (static lib)    │
                       │ ┌──────────┬──────────────┐  │
                       │ │  render  │   physics    │  │
                       │ │  ECS     │   net (Asio) │  │
                       │ │  scene   │   editor     │  │
                       │ │  assets  │   input      │  │
                       │ └──────────┴──────────────┘  │
                       └────────────────┬─────────────┘
                                        │
            Vulkan 1.2 ─ KHR_RT ─ SDL2 ─ GLFW ─ ImGui ─ Assimp ─ Boost.Asio
```

## Feature matrix

| System              | Implementation                                                          |
|---------------------|-------------------------------------------------------------------------|
| Windowing / input   | SDL2 primary, GLFW secondary path, GLUT fallback                        |
| Renderer            | Vulkan 1.2; forward PBR pass + KHR ray-tracing pipeline                 |
| Mesh loading        | Assimp; positions / normals / tangents / UVs; PBR material extraction   |
| Physics             | Sequential-impulse rigid bodies, sphere/box/plane colliders, broadphase |
| Networking          | Boost.Asio, authoritative server, snapshot replication, lag compensation|
| Editor              | ImGui hierarchy + inspector + transform gizmo + scene save/load (JSON)  |
| Scene serialization | nlohmann/json                                                           |
| ECS                 | Sparse-set component storage, archetype-free, deterministic iteration   |
| Math                | Header-only vec/mat/quat with SIMD-friendly layout                      |

## Building Cryogen

## Prerequisites

| Dependency       | Minimum   | Notes                                              |
|------------------|-----------|----------------------------------------------------|
| C++ compiler     | C++20     | GCC 12, Clang 15, or MSVC 19.36+                   |
| CMake            | 3.22      |                                                    |
| Vulkan SDK       | 1.3.250+  | Provides `glslangValidator` and validation layers  |
| Boost            | 1.74      | `system` component required (Asio header-only)     |
| SDL2             | 2.0.20    |                                                    |
| GLFW             | 3.3       | Secondary windowing path                           |
| FreeGLUT         | optional  | Legacy fallback for tooling                        |
| Assimp           | 5.2       |                                                    |
| ImGui            | 1.89+     | Vendored — see `third_party/README.md`             |
| nlohmann/json    | 3.11      | Vendored                                           |
| RenderDoc        | optional  | For GPU debugging                                  |

## Linux (Ubuntu / Debian)

```bash
sudo apt install build-essential cmake ninja-build git \
  libsdl2-dev libglfw3-dev freeglut3-dev libassimp-dev \
  libboost-system-dev

# Vulkan SDK from LunarG, installed under /opt/vulkan
export VULKAN_SDK=/opt/vulkan/x86_64
export PATH=$VULKAN_SDK/bin:$PATH
export LD_LIBRARY_PATH=$VULKAN_SDK/lib:$LD_LIBRARY_PATH

git clone <this-repo> cryogen && cd cryogen
git submodule update --init    # vendors ImGui + nlohmann/json
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## macOS

Vulkan is provided by **MoltenVK** (shipped with the LunarG SDK). The
KHR ray-tracing pipeline is **not** available on Metal; configure with
`-DCRYOGEN_ENABLE_RT=OFF` to fall back to the PBR raster path only.

```bash
brew install cmake ninja boost sdl2 glfw assimp
# Install LunarG Vulkan SDK from https://vulkan.lunarg.com
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCRYOGEN_ENABLE_RT=OFF
cmake --build build -j
```

## Windows (MSVC)

Use vcpkg to provide non-Vulkan dependencies:

```powershell
vcpkg install boost-system sdl2 glfw3 assimp nlohmann-json --triplet x64-windows
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake
cmake --build build --config Release
```

## Build options

| Option                       | Default | Description                                  |
|------------------------------|---------|----------------------------------------------|
| `CRYOGEN_BUILD_EDITOR`       | ON      | Build the ImGui editor                       |
| `CRYOGEN_BUILD_RUNTIME`      | ON      | Build the standalone client                  |
| `CRYOGEN_BUILD_SERVER`       | ON      | Build the dedicated server                   |
| `CRYOGEN_BUILD_TESTS`        | ON      | Build unit tests                             |
| `CRYOGEN_ENABLE_RT`          | ON      | Compile the KHR ray tracing pipeline         |
| `CRYOGEN_ENABLE_VALIDATION`  | ON      | Enable Vulkan validation layers in debug     |

## Vendored dependencies

Run once after cloning:

```bash
mkdir -p third_party && cd third_party
git clone --depth 1 --branch v1.90.4 https://github.com/ocornut/imgui.git
git clone --depth 1 --branch v3.11.3 https://github.com/nlohmann/json.git
```

The CMake configuration will detect both automatically.

## RenderDoc capture

Launch the editor or runtime client through RenderDoc with the working
directory set to the build root so SPIR-V binaries resolve correctly:

```
$ renderdoccmd capture build/apps/runtime/cryogen_runtime
```

## Troubleshooting

- **`glslangValidator not found`** — ensure `$VULKAN_SDK/bin` is on `PATH`.
- **Validation errors about ray tracing** — your GPU/driver may not expose
  `VK_KHR_ray_tracing_pipeline`; configure with `-DCRYOGEN_ENABLE_RT=OFF`.
- **`Boost.System` link errors** — install `libboost-system-dev` (Linux) or
  add `boost-system` to your vcpkg manifest.


## Run

```bash
# Headless authoritative server
./build/apps/server/cryogen-server --tcp 27015 --udp 27016 --snap 30

# Client (offline)
./build/apps/runtime/cryogen-runtime

# Client (connect to server)
./build/apps/runtime/cryogen-runtime 127.0.0.1

# Editor
./build/apps/editor/cryogen-editor
```