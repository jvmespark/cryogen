//
// tests/test_net_replication.cpp
// Wire-format round-trip tests for the replication payloads.
//
#include "test_util.hpp"

#include "cryogen/net/message.hpp"
#include "cryogen/net/replication.hpp"

using namespace cryogen;
using namespace cryogen::net;

static void state_round_trip() {
    std::vector<ReplicatedState> in;
    in.push_back({ /*net_id*/ 7, /*flags*/ 1,
                   /*pos*/ {1.f, 2.f, 3.f},
                   /*rot*/ math::quat{0.f, 0.7071f, 0.f, 0.7071f},
                   /*vel*/ {0.5f, -0.25f, 0.f},
                   /*hp*/  100 });
    in.push_back({ /*net_id*/ 8, /*flags*/ 1,
                   {-4.f, 0.5f, 9.f},
                   math::quat::identity(),
                   {0.f, 0.f, 0.f},
                   42 });

    BinaryWriter w;
    serialize_state(w, in);

    BinaryReader r(w.data().data(), w.data().size());
    auto out = deserialize_state(r);
    CRYOGEN_CHECK(r.ok());
    CRYOGEN_CHECK(out.size() == in.size());
    for (size_t i = 0; i < in.size(); ++i) {
        CRYOGEN_CHECK(out[i].net_id == in[i].net_id);
        CRYOGEN_CHECK(out[i].flags  == in[i].flags);
        CRYOGEN_CHECK(approx_eq(out[i].position.x, in[i].position.x));
        CRYOGEN_CHECK(approx_eq(out[i].rotation.w, in[i].rotation.w));
        CRYOGEN_CHECK(approx_eq(out[i].linear_velocity.y, in[i].linear_velocity.y));
        CRYOGEN_CHECK(out[i].hp == in[i].hp);
    }
}

static void input_round_trip() {
    InputCommand in;
    in.sequence = 12345;
    in.dt       = 1.0f / 60.0f;
    in.move     = {1.f, 0.f, -1.f};
    in.look     = {0.01f, 0.02f, 0.f};
    in.buttons  = 0b1011;

    BinaryWriter w;
    serialize_input(w, in);

    BinaryReader r(w.data().data(), w.data().size());
    auto out = deserialize_input(r);
    CRYOGEN_CHECK(r.ok());
    CRYOGEN_CHECK(out.sequence == in.sequence);
    CRYOGEN_CHECK(approx_eq(out.dt, in.dt));
    CRYOGEN_CHECK(approx_eq(out.move.z, in.move.z));
    CRYOGEN_CHECK(out.buttons == in.buttons);
}

static void header_layout_stable() {
    static_assert(sizeof(MessageHeader) == 16,
                  "MessageHeader is part of the wire format and must stay 16 bytes.");
    MessageHeader h{};
    h.type = static_cast<u16>(MessageType::Snapshot);
    h.flags = 0;
    h.sequence = 999;
    h.payload_len = 0;
    BinaryWriter w; w.write_pod(h);
    CRYOGEN_CHECK(w.data().size() == 16);
}

int main() {
    state_round_trip();
    input_round_trip();
    header_layout_stable();
    std::printf("test_net_replication: OK\n");
    return 0;
}
