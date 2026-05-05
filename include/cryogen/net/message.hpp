#pragma once
//
// cryogen/net/message.hpp
// Wire format used by both TCP control channel and UDP datagram channel.
//
// Frame:
//   uint16 type           -- MessageType
//   uint16 flags          -- bit 0: reliable; bit 1: compressed (reserved)
//   uint32 sequence
//   uint32 ack
//   uint32 payload_len
//   <payload bytes>
//

#include <cstring>
#include <string>
#include <vector>

#include "cryogen/core/types.hpp"

namespace cryogen::net {

enum class MessageType : u16 {
    LoginRequest   = 1,
    LoginAccept    = 2,
    LoginReject    = 3,
    InputCommand   = 10,    // client -> server, per-tick player input
    Snapshot       = 11,    // server -> client, world state delta
    Rpc            = 12,    // server <-> client, generic remote call
    Chat           = 13,
    Disconnect     = 99,
};

struct MessageHeader {
    u16 type        = 0;
    u16 flags       = 0;
    u32 sequence    = 0;
    u32 ack         = 0;
    u32 payload_len = 0;
};
static_assert(sizeof(MessageHeader) == 16, "MessageHeader must be wire-stable");

constexpr usize kHeaderBytes = sizeof(MessageHeader);

// ---- Binary writer / reader ---------------------------------------------
class BinaryWriter {
public:
    template <class T> void write_pod(const T& v) {
        const auto* p = reinterpret_cast<const u8*>(&v);
        buf_.insert(buf_.end(), p, p + sizeof(T));
    }
    void write_u8 (u8  v) { buf_.push_back(v); }
    void write_u16(u16 v) { write_pod(v); }
    void write_u32(u32 v) { write_pod(v); }
    void write_f32(f32 v) { write_pod(v); }
    void write_string(const std::string& s) {
        write_u32(static_cast<u32>(s.size()));
        buf_.insert(buf_.end(), s.begin(), s.end());
    }
    const std::vector<u8>& data() const { return buf_; }
    std::vector<u8>&&      take()       { return std::move(buf_); }
private:
    std::vector<u8> buf_;
};

class BinaryReader {
public:
    BinaryReader(const u8* data, usize len) : data_(data), len_(len) {}
    bool ok() const { return ok_; }
    usize remaining() const { return len_ - pos_; }

    template <class T> T read_pod() {
        T v{};
        if (pos_ + sizeof(T) > len_) { ok_ = false; return v; }
        std::memcpy(&v, data_ + pos_, sizeof(T));
        pos_ += sizeof(T);
        return v;
    }
    u8  read_u8 () { return read_pod<u8>(); }
    u16 read_u16() { return read_pod<u16>(); }
    u32 read_u32() { return read_pod<u32>(); }
    f32 read_f32() { return read_pod<f32>(); }
    std::string read_string() {
        const u32 n = read_u32();
        if (!ok_ || pos_ + n > len_) { ok_ = false; return {}; }
        std::string s(reinterpret_cast<const char*>(data_ + pos_), n);
        pos_ += n;
        return s;
    }

private:
    const u8* data_ = nullptr;
    usize     len_  = 0;
    usize     pos_  = 0;
    bool      ok_   = true;
};

} // namespace cryogen::net
