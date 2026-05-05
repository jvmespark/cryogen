#pragma once
//
// cryogen/net/server.hpp
// Authoritative game server. TCP socket per client for control messages
// (login, RPC, chat) plus a shared UDP socket for high-frequency snapshot
// state and input. Runs its own ASIO io_context on a worker thread.
//

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "cryogen/core/types.hpp"
#include "cryogen/net/replication.hpp"

namespace cryogen::ecs { class World; }

namespace cryogen::net {

struct ServerConfig {
    u16 tcp_port = 27015;
    u16 udp_port = 27016;
    u32 snapshot_hz = 30;     // server -> client state rate
    u32 lag_compensation_ticks = 8;
};

class Server {
public:
    explicit Server(ServerConfig cfg);
    ~Server();

    Server(const Server&)            = delete;
    Server& operator=(const Server&) = delete;

    // Lifecycle.
    void start();
    void stop();
    bool running() const { return running_.load(); }

    // Per-tick from the game loop:
    //   - drain inbound input commands and apply them to the simulated world
    //   - sample world state and broadcast to connected clients (rate-limited)
    void poll(ecs::World& world, double sim_time_seconds);

    // Stats.
    u32 client_count() const;

    // Hooks.
    using ChatHandler  = std::function<void(NetClientId, const std::string&)>;
    using InputHandler = std::function<void(NetClientId, const InputCommand&)>;
    void on_chat (ChatHandler  h) { chat_handler_  = std::move(h); }
    void on_input(InputHandler h) { input_handler_ = std::move(h); }

private:
    struct Impl;
    std::unique_ptr<Impl>   impl_;
    ServerConfig            cfg_;
    std::atomic<bool>       running_{false};
    std::thread             worker_;
    ChatHandler             chat_handler_;
    InputHandler            input_handler_;
};

} // namespace cryogen::net
