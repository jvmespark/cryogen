# src/CMakeLists.txt

set(CRYOGEN_SOURCES
    core/application.cpp
    core/log.cpp
    core/timer.cpp
    core/window.cpp

    math/transform.cpp

    ecs/world.cpp

    render/renderer.cpp
    render/vulkan_context.cpp
    render/swapchain.cpp
    render/pipeline.cpp
    render/mesh.cpp
    render/camera.cpp
    render/ray_tracing.cpp
    render/pbr_pass.cpp

    assets/mesh_loader.cpp
    assets/asset_manager.cpp

    physics/physics_world.cpp

    net/server.cpp
    net/client.cpp
    net/replication.cpp

    input/input.cpp

    scene/scene.cpp
    scene/serializer.cpp
)

if(CRYOGEN_BUILD_EDITOR)
    list(APPEND CRYOGEN_SOURCES editor/editor.cpp)
endif()

add_library(cryogen STATIC ${CRYOGEN_SOURCES})
add_library(cryogen::cryogen ALIAS cryogen)

target_include_directories(cryogen
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
)

target_link_libraries(cryogen
    PUBLIC
        Vulkan::Vulkan
        Boost::system
        SDL2::SDL2
        assimp::assimp
        nlohmann_json::nlohmann_json
    PRIVATE
        cryogen_warnings
)

if(CRYOGEN_BUILD_EDITOR AND TARGET imgui)
    target_link_libraries(cryogen PUBLIC imgui)
    target_compile_definitions(cryogen PUBLIC CRYOGEN_HAS_IMGUI=1)
endif()

if(CRYOGEN_ENABLE_RT)
    target_compile_definitions(cryogen PUBLIC CRYOGEN_ENABLE_RT=1)
endif()
if(CRYOGEN_ENABLE_VALIDATION)
    target_compile_definitions(cryogen PUBLIC CRYOGEN_ENABLE_VALIDATION=1)
endif()

# Threads (used by Boost.Asio io_context worker threads).
find_package(Threads REQUIRED)
target_link_libraries(cryogen PUBLIC Threads::Threads)
