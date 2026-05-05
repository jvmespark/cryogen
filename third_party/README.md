# Vendored third-party libraries

CryoGen vendors two header-only / drop-in libraries here:

```
third_party/
├── imgui/   ← https://github.com/ocornut/imgui (1.89+)
└── json/    ← https://github.com/nlohmann/json (3.11+)
```

After cloning, populate them once:

```bash
cd third_party
git clone --depth 1 --branch v1.90.4 https://github.com/ocornut/imgui.git
git clone --depth 1 --branch v3.11.3 https://github.com/nlohmann/json.git
```

Or use submodules. both layouts are detected by the top-level CMake.

The CMake build will:

- Compile `imgui/*.cpp` plus `imgui/backends/imgui_impl_sdl2.cpp` and
  `imgui/backends/imgui_impl_vulkan.cpp` into a static `imgui` target.
- Add `json/include` to the include path of an interface target
  `nlohmann_json`.

If `imgui/` is absent the editor target is skipped automatically; the
runtime client and server still build.
