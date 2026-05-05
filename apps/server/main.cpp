// apps/server/main.cpp
// Headless authoritative server. Runs the engine loop with no window or
// renderer. only ECS, physics, and the network layer. Optional command-line
// flags configure ports and snapshot rate.
//
//   cryogen-server [--tcp 27015] [--udp 27016] [--snap 30]
//
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <thread>

#include "cryogen/assets/asset_manager.hpp"
#include "cryogen/core/log.hpp"
#include "cryogen/core/timer.hpp"
#include "cryogen/ecs/world.hpp"
#include "cryogen/net/server.hpp"
#include "cryogen/physics/physics_world.hpp"
#include "cryogen/scene/components.hpp"

using namespace cryogen;

static volatile std::sig_atomic_t g_shutdown = 0;
extern "C" void on_sigint(int) { g_shutdown = 1; }

int main(int argc, char** argv) {
    net::ServerConfig cfg;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--tcp")  && i+1 < argc) cfg.tcp_port = static_cast<u16>(std::atoi(argv[++i]));
        else if (!std::strcmp(argv[i], "--udp")  && i+1 < argc) cfg.udp_port = static_cast<u16>(std::atoi(argv[++i]));
        else if (!std::strcmp(argv[i], "--snap") && i+1 < argc) cfg.snapshot_hz = static_cast<u32>(std::atoi(argv[++i]));
    }

    std::signal(SIGINT,  on_sigint);
    std::signal(SIGTERM, on_sigint);

    // Authoritative simulation: world + physics, no renderer/assets.
    ecs::World            world;
    physics::PhysicsWorld physics;

    // Spawn a tiny default scene: a static floor and a dynamic crate. The
    // server doesn't care about meshes, only colliders + transforms.
    {
        auto floor = world.create();
        scene::TransformComponent ftc;
        world.add<scene::TransformComponent>(floor, ftc);
        world.add<scene::NameComponent>(floor, {"Floor"});
        world.add<physics::ColliderComponent>(floor, physics::make_plane({0,1,0}, 0.f));
        world.add<physics::RigidBodyComponent>(floor, physics::static_body());

        auto crate = world.create();
        scene::TransformComponent ctc;
        ctc.local.position = {0.f, 5.f, 0.f};
        world.add<scene::TransformComponent>(crate, ctc);
        world.add<scene::NameComponent>(crate, {"Crate"});
        world.add<physics::ColliderComponent>(crate, physics::make_box({0.5f,0.5f,0.5f}));
        world.add<physics::RigidBodyComponent>(crate, physics::dynamic_body(1.f));
        world.add<scene::NetReplicated>(crate, {1, 0});  // net_id 1
    }

    net::Server server(cfg);
    server.on_chat([](NetClientId id, const std::string& msg) {
        CRYOGEN_INFO("chat", "client ", id, ": ", msg);
    });
    server.on_input([](NetClientId, const net::InputCommand&) {
        // projectiles/etc would be applied here.
    });
    server.start();

    CRYOGEN_INFO("server", "Tick rate: 60 Hz, snapshot rate: ", cfg.snapshot_hz, " Hz");

    constexpr float dt = 1.f / 60.f;
    core::Timer wall;
    double sim_t = 0.0;
    while (!g_shutdown) {
        physics.step(world, dt);
        sim_t += dt;
        server.poll(world, sim_t);

        // Sleep until the next tick.
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    CRYOGEN_INFO("server", "Shutting down");
    server.stop();
    (void)wall;
    return 0;
}
