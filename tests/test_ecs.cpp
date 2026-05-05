//
// tests/test_ecs.cpp
// ECS smoke tests: lifecycle, componentwise add/remove, generation versioning,
// and join iteration.
//
#include "test_util.hpp"

#include "cryogen/ecs/world.hpp"

using namespace cryogen;
using namespace cryogen::ecs;

struct Position { float x, y, z; };
struct Velocity { float x, y, z; };

static void create_destroy() {
    World w;
    auto a = w.create();
    auto b = w.create();
    CRYOGEN_CHECK(w.alive(a));
    CRYOGEN_CHECK(w.alive(b));
    w.destroy(a);
    CRYOGEN_CHECK(!w.alive(a));
    // generation should bump on reuse
    auto c = w.create();
    CRYOGEN_CHECK(w.alive(c));
    if (c.index() == a.index()) CRYOGEN_CHECK(c.generation() != a.generation());
}

static void add_get_remove() {
    World w;
    auto e = w.create();
    w.add<Position>(e, Position{1, 2, 3});
    auto* p = w.try_get<Position>(e);
    CRYOGEN_CHECK(p != nullptr);
    CRYOGEN_CHECK(approx_eq(p->y, 2.f));

    w.remove<Position>(e);
    CRYOGEN_CHECK(w.try_get<Position>(e) == nullptr);
}

static void each_join() {
    World w;
    for (int i = 0; i < 10; ++i) {
        auto e = w.create();
        w.add<Position>(e, Position{ float(i), 0, 0 });
        if (i % 2 == 0)
            w.add<Velocity>(e, Velocity{1, 0, 0});
    }

    int matches = 0;
    w.each<Position, Velocity>([&](Entity, Position&, Velocity&) {
        ++matches;
    });
    CRYOGEN_CHECK(matches == 5);
}

static void destroy_invalidates_components() {
    World w;
    auto e = w.create();
    w.add<Position>(e, {1, 1, 1});
    w.destroy(e);
    CRYOGEN_CHECK(w.try_get<Position>(e) == nullptr);
}

int main() {
    create_destroy();
    add_get_remove();
    each_join();
    destroy_invalidates_components();
    std::printf("test_ecs: OK\n");
    return 0;
}
