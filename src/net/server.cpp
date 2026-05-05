//
// cryogen/net/server.cpp
//
// Boost.Asio implementation. The server runs a worker thread that owns
// io_context. Game-loop code calls Server::poll() from the main thread to
// drive snapshots and consume input. All shared state goes through `mu_`.
//
#include "cryogen/net/server.hpp"

#include <algorithm>
#include <chrono>
#include <deque>
#include <map>
#include <memory>
#include <mutex>

#include <boost/asio.hpp>

#include "cryogen/core/log.hpp"
#include "cryogen/ecs/world.hpp"

namespace cryogen::net {

namespace asio = boost::asio;
using tcp_socket = asio::ip::tcp::socket;
using udp_socket = asio::ip::udp::socket;

struct ClientSession : std::enable_shared_from_this<ClientSession> {
    NetClientId           id   = kInvalidClient;
    tcp_socket            sock;
    asio::ip::udp::endpoint udp_endpoint;
    std::string           name;

    // Pending TCP reads.
    std::vector<u8>       in_buf;
    MessageHeader         in_hdr{};
    bool                  reading_payload = false;

    explicit ClientSession(asio::io_context& io) : sock(io) {}
};

struct Server::Impl {
    explicit Impl(asio::io_context& io, ServerConfig cfg)
        : io(io),
          tcp_acceptor(io, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), cfg.tcp_port)),
          udp(io, asio::ip::udp::endpoint(asio::ip::udp::v4(), cfg.udp_port)) {}

    asio::io_context&            io;
    asio::ip::tcp::acceptor      tcp_acceptor;
    udp_socket                   udp;
    std::array<u8, 2048>         udp_buf;
    asio::ip::udp::endpoint      udp_remote;

    std::mutex                                       mu;
    NetClientId                                      next_id = 1;
    u32                                              next_net_entity_id = 1;
    std::map<NetClientId, std::shared_ptr<ClientSession>> clients;
    std::deque<std::pair<NetClientId, InputCommand>> input_queue;
    std::deque<std::pair<NetClientId, std::string>>  chat_queue;
    double                                           last_snapshot_t = 0.0;
};

namespace {
void send_tcp(std::shared_ptr<ClientSession> s, MessageType type, std::vector<u8> payload, u32 seq = 0) {
    MessageHeader h;
    h.type = static_cast<u16>(type);
    h.flags = 0x1;   // reliable
    h.sequence = seq;
    h.payload_len = static_cast<u32>(payload.size());

    std::vector<u8> frame(kHeaderBytes + payload.size());
    std::memcpy(frame.data(), &h, kHeaderBytes);
    if (!payload.empty())
        std::memcpy(frame.data() + kHeaderBytes, payload.data(), payload.size());

    auto buf = std::make_shared<std::vector<u8>>(std::move(frame));
    asio::async_write(s->sock, asio::buffer(*buf),
        [s, buf](const boost::system::error_code& ec, std::size_t) {
            if (ec) {
                CRYOGEN_WARN("net", "TCP write failed for client ", s->id, ": ", ec.message());
            }
        });
}

void start_tcp_read(std::shared_ptr<ClientSession> s,
                    Server::Impl& impl,
                    std::function<void(NetClientId, MessageHeader, std::vector<u8>)> on_msg);

void read_payload(std::shared_ptr<ClientSession> s,
                  Server::Impl& impl,
                  std::function<void(NetClientId, MessageHeader, std::vector<u8>)> on_msg) {
    s->in_buf.assign(s->in_hdr.payload_len, 0);
    if (s->in_hdr.payload_len == 0) {
        on_msg(s->id, s->in_hdr, {});
        s->reading_payload = false;
        start_tcp_read(s, impl, std::move(on_msg));
        return;
    }
    asio::async_read(s->sock, asio::buffer(s->in_buf),
        [s, &impl, on_msg](const boost::system::error_code& ec, std::size_t) mutable {
            if (ec) {
                CRYOGEN_INFO("net", "Client ", s->id, " disconnected: ", ec.message());
                std::lock_guard<std::mutex> g(impl.mu);
                impl.clients.erase(s->id);
                return;
            }
            on_msg(s->id, s->in_hdr, std::move(s->in_buf));
            s->reading_payload = false;
            start_tcp_read(s, impl, std::move(on_msg));
        });
}

void start_tcp_read(std::shared_ptr<ClientSession> s,
                    Server::Impl& impl,
                    std::function<void(NetClientId, MessageHeader, std::vector<u8>)> on_msg) {
    asio::async_read(s->sock, asio::buffer(&s->in_hdr, kHeaderBytes),
        [s, &impl, on_msg](const boost::system::error_code& ec, std::size_t) mutable {
            if (ec) {
                CRYOGEN_INFO("net", "Client ", s->id, " disconnected: ", ec.message());
                std::lock_guard<std::mutex> g(impl.mu);
                impl.clients.erase(s->id);
                return;
            }
            s->reading_payload = true;
            read_payload(s, impl, std::move(on_msg));
        });
}

void start_udp_recv(Server::Impl& impl, ServerConfig& /*cfg*/);

void handle_udp_packet(Server::Impl& impl, ServerConfig& cfg, std::size_t len) {
    if (len < kHeaderBytes) { start_udp_recv(impl, cfg); return; }
    MessageHeader h{};
    std::memcpy(&h, impl.udp_buf.data(), kHeaderBytes);
    if (h.payload_len + kHeaderBytes > len) { start_udp_recv(impl, cfg); return; }

    BinaryReader r(impl.udp_buf.data() + kHeaderBytes, h.payload_len);
    if (static_cast<MessageType>(h.type) == MessageType::InputCommand) {
        // Find the session by remembering the source endpoint on first packet.
        std::lock_guard<std::mutex> g(impl.mu);
        for (auto& [id, s] : impl.clients) {
            if (s->udp_endpoint.address().is_unspecified() ||
                s->udp_endpoint == impl.udp_remote) {
                s->udp_endpoint = impl.udp_remote;
                InputCommand cmd = deserialize_input(r);
                impl.input_queue.emplace_back(id, cmd);
                break;
            }
        }
    }
    start_udp_recv(impl, cfg);
}

void start_udp_recv(Server::Impl& impl, ServerConfig& cfg) {
    impl.udp.async_receive_from(asio::buffer(impl.udp_buf), impl.udp_remote,
        [&impl, &cfg](const boost::system::error_code& ec, std::size_t len) {
            if (!ec) handle_udp_packet(impl, cfg, len);
            else     start_udp_recv(impl, cfg);
        });
}
} // namespace

Server::Server(ServerConfig cfg) : cfg_(cfg) {}
Server::~Server() { stop(); }

void Server::start() {
    if (running_.load()) return;
    running_.store(true);

    worker_ = std::thread([this]() {
        try {
            asio::io_context io;
            impl_ = std::make_unique<Impl>(io, cfg_);
            CRYOGEN_INFO("net", "Server listening tcp/", cfg_.tcp_port, " udp/", cfg_.udp_port);

            auto on_msg = [this](NetClientId id, MessageHeader h, std::vector<u8> payload) {
                BinaryReader r(payload.data(), payload.size());
                switch (static_cast<MessageType>(h.type)) {
                    case MessageType::LoginRequest: {
                        std::string name = r.read_string();
                        std::lock_guard<std::mutex> g(impl_->mu);
                        auto it = impl_->clients.find(id);
                        if (it != impl_->clients.end()) it->second->name = name;
                        BinaryWriter w; w.write_u32(id);
                        send_tcp(it->second, MessageType::LoginAccept, w.take(), h.sequence);
                        CRYOGEN_INFO("net", "Client ", id, " logged in as '", name, "'");
                        break;
                    }
                    case MessageType::Chat: {
                        std::string msg = r.read_string();
                        if (chat_handler_) chat_handler_(id, msg);
                        std::lock_guard<std::mutex> g(impl_->mu);
                        impl_->chat_queue.emplace_back(id, msg);
                        break;
                    }
                    case MessageType::Disconnect: {
                        std::lock_guard<std::mutex> g(impl_->mu);
                        impl_->clients.erase(id);
                        break;
                    }
                    default: break;
                }
            };

            std::function<void()> do_accept;
            do_accept = [&]() {
                auto session = std::make_shared<ClientSession>(io);
                impl_->tcp_acceptor.async_accept(session->sock,
                    [this, session, &do_accept, on_msg](const boost::system::error_code& ec) mutable {
                        if (!ec) {
                            std::lock_guard<std::mutex> g(impl_->mu);
                            session->id = impl_->next_id++;
                            impl_->clients.emplace(session->id, session);
                            CRYOGEN_INFO("net", "TCP accept: client ", session->id);
                            start_tcp_read(session, *impl_, on_msg);
                        }
                        do_accept();
                    });
            };

            do_accept();
            start_udp_recv(*impl_, cfg_);

            while (running_.load()) {
                io.run_for(std::chrono::milliseconds(10));
                io.restart();
            }
            io.stop();
            impl_.reset();
        } catch (const std::exception& e) {
            CRYOGEN_ERROR("net", "Server worker exception: ", e.what());
        }
    });
}

void Server::stop() {
    if (!running_.load()) return;
    running_.store(false);
    if (worker_.joinable()) worker_.join();
}

void Server::poll(ecs::World& world, double sim_time_seconds) {
    if (!impl_) return;

    // 1) Drain input queue (called from main thread -> apply to sim authoritatively).
    std::deque<std::pair<NetClientId, InputCommand>> drained;
    {
        std::lock_guard<std::mutex> g(impl_->mu);
        drained.swap(impl_->input_queue);
    }
    if (input_handler_) {
        for (auto& [id, cmd] : drained) input_handler_(id, cmd);
    }

    // 2) Snapshot rate-limited broadcast.
    const double interval = 1.0 / static_cast<double>(cfg_.snapshot_hz);
    if (sim_time_seconds - impl_->last_snapshot_t < interval) return;
    impl_->last_snapshot_t = sim_time_seconds;

    auto states = snapshot_world(world);
    BinaryWriter w;
    serialize_state(w, states);
    auto payload = w.take();

    MessageHeader h{};
    h.type = static_cast<u16>(MessageType::Snapshot);
    h.flags = 0;
    h.sequence = static_cast<u32>(impl_->last_snapshot_t * 1000.0);
    h.payload_len = static_cast<u32>(payload.size());

    std::vector<u8> frame(kHeaderBytes + payload.size());
    std::memcpy(frame.data(), &h, kHeaderBytes);
    if (!payload.empty())
        std::memcpy(frame.data() + kHeaderBytes, payload.data(), payload.size());

    std::lock_guard<std::mutex> g(impl_->mu);
    boost::system::error_code ec;
    for (auto& [id, s] : impl_->clients) {
        if (s->udp_endpoint.address().is_unspecified()) continue;
        impl_->udp.send_to(asio::buffer(frame), s->udp_endpoint, 0, ec);
    }
}

u32 Server::client_count() const {
    if (!impl_) return 0;
    std::lock_guard<std::mutex> g(impl_->mu);
    return static_cast<u32>(impl_->clients.size());
}

} // namespace cryogen::net
