cmake_minimum_required(VERSION 3.22)
project(CryoGen
    VERSION     0.1.0
    DESCRIPTION "Vulkan game engine with multiplayer, physics, ray tracing, and editor"
    LANGUAGES   CXX
)

# ------------------------------------------------------------------
# Project options
# ------------------------------------------------------------------
option(CRYOGEN_BUILD_EDITOR   "Build the editor application"           ON)
option(CRYOGEN_BUILD_RUNTIME  "Build the standalone runtime client"    ON)
option(CRYOGEN_BUILD_SERVER   "Build the dedicated server"             ON)
option(CRYOGEN_BUILD_TESTS    "Build unit tests"                       ON)
option(CRYOGEN_ENABLE_RT      "Enable VK_KHR_ray_tracing_pipeline path" ON)
option(CRYOGEN_ENABLE_VALIDATION "Enable Vulkan validation layers"     ON)

set(CMAKE_CXX_STANDARD          20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)

# ------------------------------------------------------------------
# Dependencies
# ------------------------------------------------------------------
find_package(Vulkan REQUIRED)

# Boost.Asio (header-only system component is preferred; we link Boost::system).
set(Boost_USE_STATIC_LIBS OFF)
find_package(Boost 1.74 REQUIRED COMPONENTS system)

find_package(SDL2  CONFIG QUIET)
if(NOT SDL2_FOUND)
    find_package(SDL2 REQUIRED)
endif()

# GLFW is supported as an alternate windowing backend (off by default; SDL2 is
# the primary). Detected if installed but not strictly required.
find_package(glfw3 CONFIG QUIET)

# GLUT is included for legacy fallback / tooling visualizations.
find_package(GLUT QUIET)

find_package(assimp CONFIG QUIET)
if(NOT assimp_FOUND)
    find_package(assimp REQUIRED)
endif()

# ImGui: vendored under third_party/imgui (see third_party/README.md).
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui")
if(EXISTS "${IMGUI_DIR}/imgui.cpp")
    add_library(imgui STATIC
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/imgui_demo.cpp
        ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
        ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
    )
    target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
    target_link_libraries(imgui PUBLIC SDL2::SDL2 Vulkan::Vulkan)
else()
    message(WARNING
        "ImGui not found at ${IMGUI_DIR}. The editor target will be skipped. "
        "Vendor ImGui per third_party/README.md to enable it.")
endif()

# nlohmann/json: header-only, vendored at third_party/json/include.
set(JSON_INC "${CMAKE_CURRENT_SOURCE_DIR}/third_party/json/include")
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE ${JSON_INC})
add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)

# ------------------------------------------------------------------
# Engine library
# ------------------------------------------------------------------
add_subdirectory(src)

# ------------------------------------------------------------------
# Apps
# ------------------------------------------------------------------
if(CRYOGEN_BUILD_EDITOR AND TARGET imgui)
    add_subdirectory(apps/editor)
endif()
if(CRYOGEN_BUILD_RUNTIME)
    add_subdirectory(apps/runtime)
endif()
if(CRYOGEN_BUILD_SERVER)
    add_subdirectory(apps/server)
endif()

# ------------------------------------------------------------------
# Shaders
# ------------------------------------------------------------------
add_subdirectory(shaders)

# ------------------------------------------------------------------
# Tests
# ------------------------------------------------------------------
if(CRYOGEN_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ------------------------------------------------------------------
# Summary
# ------------------------------------------------------------------
message(STATUS "------------------------------------------------------------")
message(STATUS " CryoGen ${PROJECT_VERSION}")
message(STATUS "   Build type        : ${CMAKE_BUILD_TYPE}")
message(STATUS "   Editor            : ${CRYOGEN_BUILD_EDITOR}")
message(STATUS "   Runtime           : ${CRYOGEN_BUILD_RUNTIME}")
message(STATUS "   Server            : ${CRYOGEN_BUILD_SERVER}")
message(STATUS "   Tests             : ${CRYOGEN_BUILD_TESTS}")
message(STATUS "   Ray tracing       : ${CRYOGEN_ENABLE_RT}")
message(STATUS "   Validation layers : ${CRYOGEN_ENABLE_VALIDATION}")
message(STATUS "------------------------------------------------------------")
