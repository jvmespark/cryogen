#pragma once
//
// cryogen/net/client.hpp
// Asynchronous Boost.Asio client.
//
// Two channels:
//   - TCP : login/RPC/chat (reliable, ordered)
//   - UDP : snapshots (server -> client) and input commands (client -> server)
//
// The client owns its own io_context and runs it on a single worker thread.
// User code interacts only via send_* / poll_* methods on the main thread,
// which are mutex-protected and therefore safe to call from the game loop.
//

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include "cryogen/core/types.hpp"
#include "cryogen/net/replication.hpp"

namespace cryogen::net {

struct ClientConfig {
    std::string host = "127.0.0.1";
    u16         tcp_port = 27015;
    u16         udp_port = 27016;
    std::string player_name = "player";
};

class Client {
public:
    explicit Client(ClientConfig cfg);
    ~Client();

    Client(const Client&)            = delete;
    Client& operator=(const Client&) = delete;

    void connect();
    void disconnect();

    bool connected() const { return connected_.load(); }
    NetClientId id() const { return id_.load(); }

    void send_input(const InputCommand& cmd);
    void send_chat(const std::string& text);

    // Drain any incoming snapshots since the last poll. Returned snapshots
    // are sorted by sequence ascending.
    std::vector<std::vector<ReplicatedState>> drain_snapshots();
    std::vector<std::string>                  drain_chat();

private:
    struct Impl;
    std::unique_ptr<Impl>      impl_;
    ClientConfig               cfg_;
    std::atomic<bool>          connected_{false};
    std::atomic<NetClientId>   id_{kInvalidClient};
    std::thread                worker_;
};

} // namespace cryogen::net
