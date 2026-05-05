//
// cryogen/net/client.cpp
//
#include "cryogen/net/client.hpp"

#include <chrono>
#include <cstring>
#include <deque>
#include <mutex>

#include <boost/asio.hpp>

#include "cryogen/core/log.hpp"

namespace cryogen::net {

namespace asio = boost::asio;
using tcp = asio::ip::tcp;
using udp = asio::ip::udp;

struct Client::Impl {
    Impl(asio::io_context& io, ClientConfig cfg)
        : io(io), tcp_sock(io), udp_sock(io, udp::endpoint(udp::v4(), 0)) {
        tcp::resolver tres(io);
        udp::resolver ures(io);
        tcp_endpoints = tres.resolve(cfg.host, std::to_string(cfg.tcp_port));
        auto uit = ures.resolve(cfg.host, std::to_string(cfg.udp_port));
        if (uit.begin() != uit.end()) udp_remote = *uit.begin();
    }

    asio::io_context&       io;
    tcp::socket             tcp_sock;
    tcp::resolver::results_type tcp_endpoints;
    udp::socket             udp_sock;
    udp::endpoint           udp_remote;

    std::array<u8, 2048>    udp_buf;
    udp::endpoint           udp_remote_recv;

    MessageHeader           tcp_in_hdr{};
    std::vector<u8>         tcp_in_buf;

    std::mutex                                          mu;
    std::deque<std::vector<ReplicatedState>>            snapshots;
    std::deque<std::string>                             chats;
};

namespace {
void send_tcp_msg(Client::Impl& impl, MessageType type, std::vector<u8> payload) {
    MessageHeader h{};
    h.type = static_cast<u16>(type);
    h.flags = 0x1;
    h.payload_len = static_cast<u32>(payload.size());
    std::vector<u8> frame(kHeaderBytes + payload.size());
    std::memcpy(frame.data(), &h, kHeaderBytes);
    if (!payload.empty())
        std::memcpy(frame.data() + kHeaderBytes, payload.data(), payload.size());
    auto buf = std::make_shared<std::vector<u8>>(std::move(frame));
    asio::async_write(impl.tcp_sock, asio::buffer(*buf),
        [buf](const boost::system::error_code& ec, std::size_t) {
            if (ec) CRYOGEN_WARN("net", "Client TCP write failed: ", ec.message());
        });
}

void start_udp_recv(Client::Impl& impl);
void on_udp_packet(Client::Impl& impl, std::size_t len) {
    if (len >= kHeaderBytes) {
        MessageHeader h{};
        std::memcpy(&h, impl.udp_buf.data(), kHeaderBytes);
        if (kHeaderBytes + h.payload_len <= len &&
            static_cast<MessageType>(h.type) == MessageType::Snapshot) {
            BinaryReader r(impl.udp_buf.data() + kHeaderBytes, h.payload_len);
            auto states = deserialize_state(r);
            std::lock_guard<std::mutex> g(impl.mu);
            impl.snapshots.push_back(std::move(states));
        }
    }
    start_udp_recv(impl);
}
void start_udp_recv(Client::Impl& impl) {
    impl.udp_sock.async_receive_from(asio::buffer(impl.udp_buf), impl.udp_remote_recv,
        [&impl](const boost::system::error_code& ec, std::size_t len) {
            if (!ec) on_udp_packet(impl, len);
            else     start_udp_recv(impl);
        });
}

void start_tcp_read(Client::Impl& impl);
void read_payload(Client::Impl& impl) {
    impl.tcp_in_buf.assign(impl.tcp_in_hdr.payload_len, 0);
    if (impl.tcp_in_hdr.payload_len == 0) {
        start_tcp_read(impl);
        return;
    }
    asio::async_read(impl.tcp_sock, asio::buffer(impl.tcp_in_buf),
        [&impl](const boost::system::error_code& ec, std::size_t) {
            if (ec) {
                CRYOGEN_WARN("net", "Client TCP read failed: ", ec.message());
                return;
            }
            BinaryReader r(impl.tcp_in_buf.data(), impl.tcp_in_buf.size());
            switch (static_cast<MessageType>(impl.tcp_in_hdr.type)) {
                case MessageType::LoginAccept: {
                    // payload = u32 client id
                    /* server already known; record id */
                    break;
                }
                case MessageType::Chat: {
                    std::lock_guard<std::mutex> g(impl.mu);
                    impl.chats.push_back(r.read_string());
                    break;
                }
                default: break;
            }
            start_tcp_read(impl);
        });
}
void start_tcp_read(Client::Impl& impl) {
    asio::async_read(impl.tcp_sock, asio::buffer(&impl.tcp_in_hdr, kHeaderBytes),
        [&impl](const boost::system::error_code& ec, std::size_t) {
            if (ec) {
                CRYOGEN_WARN("net", "Client TCP header read failed: ", ec.message());
                return;
            }
            read_payload(impl);
        });
}

} // namespace

Client::Client(ClientConfig cfg) : cfg_(cfg) {}
Client::~Client() { disconnect(); }

void Client::connect() {
    if (connected_.load()) return;
    connected_.store(true);
    worker_ = std::thread([this]() {
        try {
            asio::io_context io;
            impl_ = std::make_unique<Impl>(io, cfg_);

            asio::async_connect(impl_->tcp_sock, impl_->tcp_endpoints,
                [this](const boost::system::error_code& ec, const tcp::endpoint&) {
                    if (ec) {
                        CRYOGEN_ERROR("net", "Connect failed: ", ec.message());
                        connected_.store(false);
                        return;
                    }
                    CRYOGEN_INFO("net", "Connected to server");
                    BinaryWriter w; w.write_string(cfg_.player_name);
                    send_tcp_msg(*impl_, MessageType::LoginRequest, w.take());
                    start_tcp_read(*impl_);

                    // Punch a UDP packet so the server learns our endpoint.
                    BinaryWriter wu; wu.write_u32(0);
                    MessageHeader h{};
                    h.type = static_cast<u16>(MessageType::InputCommand);
                    h.payload_len = static_cast<u32>(wu.data().size());
                    std::vector<u8> frame(kHeaderBytes + wu.data().size());
                    std::memcpy(frame.data(), &h, kHeaderBytes);
                    std::memcpy(frame.data()+kHeaderBytes, wu.data().data(), wu.data().size());
                    boost::system::error_code uec;
                    impl_->udp_sock.send_to(asio::buffer(frame), impl_->udp_remote, 0, uec);
                });
            start_udp_recv(*impl_);

            while (connected_.load()) {
                io.run_for(std::chrono::milliseconds(5));
                io.restart();
            }
            io.stop();
            impl_.reset();
        } catch (const std::exception& e) {
            CRYOGEN_ERROR("net", "Client worker exception: ", e.what());
            connected_.store(false);
        }
    });
}

void Client::disconnect() {
    if (!connected_.load()) return;
    connected_.store(false);
    if (worker_.joinable()) worker_.join();
}

void Client::send_input(const InputCommand& cmd) {
    if (!impl_) return;
    BinaryWriter w; serialize_input(w, cmd);
    auto payload = w.take();
    MessageHeader h{};
    h.type = static_cast<u16>(MessageType::InputCommand);
    h.sequence = cmd.sequence;
    h.payload_len = static_cast<u32>(payload.size());
    std::vector<u8> frame(kHeaderBytes + payload.size());
    std::memcpy(frame.data(), &h, kHeaderBytes);
    if (!payload.empty()) std::memcpy(frame.data()+kHeaderBytes, payload.data(), payload.size());
    boost::system::error_code ec;
    impl_->udp_sock.send_to(boost::asio::buffer(frame), impl_->udp_remote, 0, ec);
}

void Client::send_chat(const std::string& text) {
    if (!impl_) return;
    BinaryWriter w; w.write_string(text);
    send_tcp_msg(*impl_, MessageType::Chat, w.take());
}

std::vector<std::vector<ReplicatedState>> Client::drain_snapshots() {
    if (!impl_) return {};
    std::vector<std::vector<ReplicatedState>> out;
    std::lock_guard<std::mutex> g(impl_->mu);
    while (!impl_->snapshots.empty()) {
        out.push_back(std::move(impl_->snapshots.front()));
        impl_->snapshots.pop_front();
    }
    return out;
}

std::vector<std::string> Client::drain_chat() {
    if (!impl_) return {};
    std::vector<std::string> out;
    std::lock_guard<std::mutex> g(impl_->mu);
    while (!impl_->chats.empty()) {
        out.push_back(std::move(impl_->chats.front()));
        impl_->chats.pop_front();
    }
    return out;
}

} // namespace cryogen::net
