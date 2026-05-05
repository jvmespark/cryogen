//
// cryogen/physics/physics_world.cpp
//
// Sequential-impulses solver after Catto, with semi-implicit Euler integration.
// Sweep-and-prune broadphase along the X-axis. Narrowphase: sphere/sphere,
// sphere/box (via box-closest-point), sphere/plane, box/plane, box/box (SAT
// with face-axis-only contact). Box/box contact reduction uses a single
// representative penetration normal
//

#include "cryogen/physics/physics_world.hpp"

#include <algorithm>
#include <cmath>

#include "cryogen/core/log.hpp"
#include "cryogen/ecs/world.hpp"
#include "cryogen/scene/components.hpp"

namespace cryogen::physics {

using cryogen::scene::TransformComponent;

namespace {

math::Aabb sphere_aabb(math::vec3 c, float r) {
    return { { c.x - r, c.y - r, c.z - r },
             { c.x + r, c.y + r, c.z + r } };
}

math::Aabb box_aabb(math::vec3 c, math::vec3 he, math::quat q) {
    // Project the 8 corners.
    math::vec3 axes[3] = {
        math::rotate(q, math::vec3{1,0,0}),
        math::rotate(q, math::vec3{0,1,0}),
        math::rotate(q, math::vec3{0,0,1}),
    };
    math::vec3 hext;
    hext.x = std::fabs(axes[0].x)*he.x + std::fabs(axes[1].x)*he.y + std::fabs(axes[2].x)*he.z;
    hext.y = std::fabs(axes[0].y)*he.x + std::fabs(axes[1].y)*he.y + std::fabs(axes[2].y)*he.z;
    hext.z = std::fabs(axes[0].z)*he.x + std::fabs(axes[1].z)*he.y + std::fabs(axes[2].z)*he.z;
    return { c - hext, c + hext };
}

// ---- Narrowphase contacts ------------------------------------------------
bool contact_sphere_sphere(const TransformComponent& at, const ColliderComponent& ac,
                           const TransformComponent& bt, const ColliderComponent& bc,
                           ContactManifold& m) {
    const math::vec3 d  = bt.local.position - at.local.position;
    const float     r  = ac.radius + bc.radius;
    const float     d2 = math::dot(d, d);
    if (d2 >= r * r) return false;
    const float dist = std::sqrt(d2);
    const math::vec3 n = (dist > 1e-6f) ? d / dist : math::vec3{0,1,0};
    m.normal = n;
    m.depth  = r - dist;
    m.point  = at.local.position + n * (ac.radius - m.depth * 0.5f);
    return true;
}

bool contact_sphere_plane(const TransformComponent& st, const ColliderComponent& sc,
                          const TransformComponent& /*pt*/, const ColliderComponent& pc,
                          ContactManifold& m) {
    const float dist = math::dot(pc.plane_normal, st.local.position) - pc.plane_offset;
    if (dist >= sc.radius) return false;
    m.normal = -pc.plane_normal;     // points from sphere -> plane
    m.depth  = sc.radius - dist;
    m.point  = st.local.position + pc.plane_normal * (-dist);
    return true;
}

bool contact_box_plane(const TransformComponent& bt, const ColliderComponent& bc,
                       const TransformComponent& /*pt*/, const ColliderComponent& pc,
                       ContactManifold& m) {
    // Project the box's half-extent onto the plane normal.
    math::vec3 axes[3] = {
        math::rotate(bt.local.rotation, math::vec3{1,0,0}),
        math::rotate(bt.local.rotation, math::vec3{0,1,0}),
        math::rotate(bt.local.rotation, math::vec3{0,0,1}),
    };
    const float r = std::fabs(math::dot(pc.plane_normal, axes[0])) * bc.half_extents.x +
                    std::fabs(math::dot(pc.plane_normal, axes[1])) * bc.half_extents.y +
                    std::fabs(math::dot(pc.plane_normal, axes[2])) * bc.half_extents.z;
    const float dist = math::dot(pc.plane_normal, bt.local.position) - pc.plane_offset;
    if (dist >= r) return false;
    m.normal = -pc.plane_normal;
    m.depth  = r - dist;
    m.point  = bt.local.position + pc.plane_normal * (-dist);
    return true;
}

bool contact_sphere_box(const TransformComponent& st, const ColliderComponent& sc,
                        const TransformComponent& bt, const ColliderComponent& bc,
                        ContactManifold& m) {
    // Transform the sphere centre into the box's local frame.
    const math::quat inv = math::conjugate(bt.local.rotation);
    const math::vec3 local = math::rotate(inv, st.local.position - bt.local.position);
    // Closest point on the box (axis-aligned in this frame).
    math::vec3 closest = local;
    closest.x = math::clamp(closest.x, -bc.half_extents.x, bc.half_extents.x);
    closest.y = math::clamp(closest.y, -bc.half_extents.y, bc.half_extents.y);
    closest.z = math::clamp(closest.z, -bc.half_extents.z, bc.half_extents.z);
    const math::vec3 d = local - closest;
    const float d2 = math::dot(d, d);
    if (d2 >= sc.radius * sc.radius) return false;
    const float dist = std::sqrt(d2);
    math::vec3 n_local;
    if (dist > 1e-6f) n_local = d / dist;
    else {
        // Sphere centre is inside the box: pick the nearest face.
        n_local = {1,0,0};
        float min_pen = bc.half_extents.x - std::fabs(local.x);
        if (bc.half_extents.y - std::fabs(local.y) < min_pen) {
            min_pen = bc.half_extents.y - std::fabs(local.y);
            n_local = {0, local.y < 0 ? -1.f : 1.f, 0};
        } else if (local.x < 0) n_local.x = -1.f;
        if (bc.half_extents.z - std::fabs(local.z) < min_pen) {
            n_local = {0, 0, local.z < 0 ? -1.f : 1.f};
        }
    }
    const math::vec3 n_world = math::normalize(math::rotate(bt.local.rotation, n_local));
    m.normal = n_world;     // from sphere outward
    m.depth  = sc.radius - dist;
    m.point  = st.local.position - n_world * (sc.radius - m.depth * 0.5f);
    return true;
}

bool contact_box_box(const TransformComponent& at, const ColliderComponent& ac,
                     const TransformComponent& bt, const ColliderComponent& bc,
                     ContactManifold& m) {
    // SAT with face axes only.
    math::vec3 axA[3] = {
        math::rotate(at.local.rotation, math::vec3{1,0,0}),
        math::rotate(at.local.rotation, math::vec3{0,1,0}),
        math::rotate(at.local.rotation, math::vec3{0,0,1}),
    };
    math::vec3 axB[3] = {
        math::rotate(bt.local.rotation, math::vec3{1,0,0}),
        math::rotate(bt.local.rotation, math::vec3{0,1,0}),
        math::rotate(bt.local.rotation, math::vec3{0,0,1}),
    };
    const math::vec3 t = bt.local.position - at.local.position;

    auto overlap_on = [&](math::vec3 axis) -> float {
        const float ra = std::fabs(math::dot(axis, axA[0])) * ac.half_extents.x +
                         std::fabs(math::dot(axis, axA[1])) * ac.half_extents.y +
                         std::fabs(math::dot(axis, axA[2])) * ac.half_extents.z;
        const float rb = std::fabs(math::dot(axis, axB[0])) * bc.half_extents.x +
                         std::fabs(math::dot(axis, axB[1])) * bc.half_extents.y +
                         std::fabs(math::dot(axis, axB[2])) * bc.half_extents.z;
        return ra + rb - std::fabs(math::dot(axis, t));
    };

    math::vec3 best_axis{0,1,0};
    float      best_overlap = 1e30f;
    auto consider = [&](math::vec3 axis) {
        if (math::length_sq(axis) < 1e-8f) return;
        axis = math::normalize(axis);
        const float o = overlap_on(axis);
        if (o < 0.f) { best_overlap = -1.f; return; }
        if (o < best_overlap) { best_overlap = o; best_axis = axis; }
    };
    for (int i = 0; i < 3 && best_overlap >= 0.f; ++i) consider(axA[i]);
    for (int i = 0; i < 3 && best_overlap >= 0.f; ++i) consider(axB[i]);

    if (best_overlap < 0.f) return false;

    // Orient axis to point from A -> B.
    if (math::dot(best_axis, t) < 0.f) best_axis = best_axis * -1.f;
    m.normal = best_axis;
    m.depth  = best_overlap;
    m.point  = at.local.position + t * 0.5f;
    return true;
}

} // namespace

// ----------------------------------------------------------------------
void PhysicsWorld::step(ecs::World& world, float dt) {
    if (dt <= 0.f) return;
    update_world_aabbs(world);
    integrate_velocities(world, dt);

    contacts_.clear();

    std::vector<std::pair<ecs::Entity, ecs::Entity>> pairs;
    broadphase_sap(world, pairs);
    narrowphase(world, pairs);
    solve(world, dt);

    integrate_positions(world, dt);
}

void PhysicsWorld::update_world_aabbs(ecs::World& world) {
    world.each<TransformComponent, ColliderComponent>(
        [](ecs::Entity, TransformComponent& tc, ColliderComponent& cc) {
            switch (cc.type) {
                case ShapeType::Sphere:
                    cc.world_aabb = sphere_aabb(tc.local.position, cc.radius);
                    break;
                case ShapeType::Box:
                    cc.world_aabb = box_aabb(tc.local.position, cc.half_extents, tc.local.rotation);
                    break;
                case ShapeType::Plane:
                    cc.world_aabb = { { -1e30f, -1e30f, -1e30f },
                                      { +1e30f, +1e30f, +1e30f } };
                    break;
            }
        });
}

void PhysicsWorld::broadphase_sap(ecs::World& world,
                                  std::vector<std::pair<ecs::Entity, ecs::Entity>>& pairs) {
    struct Endpoint { float x; ecs::Entity e; bool is_min; };
    std::vector<Endpoint> eps;
    eps.reserve(world.entity_count() * 2);

    world.each<ColliderComponent>(
        [&](ecs::Entity e, ColliderComponent& cc) {
            eps.push_back({cc.world_aabb.min.x, e, true});
            eps.push_back({cc.world_aabb.max.x, e, false});
        });
    std::sort(eps.begin(), eps.end(), [](auto& a, auto& b){ return a.x < b.x; });

    std::vector<ecs::Entity> active;
    for (auto& ep : eps) {
        if (ep.is_min) {
            for (auto& other : active) {
                auto* ca = world.try_get<ColliderComponent>(ep.e);
                auto* cb = world.try_get<ColliderComponent>(other);
                if (ca && cb && math::overlaps(ca->world_aabb, cb->world_aabb)) {
                    // canonicalize ordering by id
                    if (ep.e.id < other.id) pairs.push_back({ep.e, other});
                    else                    pairs.push_back({other, ep.e});
                }
            }
            active.push_back(ep.e);
        } else {
            active.erase(std::remove_if(active.begin(), active.end(),
                                        [&](ecs::Entity x){ return x == ep.e; }),
                         active.end());
        }
    }
}

void PhysicsWorld::narrowphase(ecs::World& world,
                               const std::vector<std::pair<ecs::Entity, ecs::Entity>>& pairs) {
    for (const auto& p : pairs) {
        auto* ta = world.try_get<TransformComponent>(p.first);
        auto* tb = world.try_get<TransformComponent>(p.second);
        auto* ca = world.try_get<ColliderComponent>(p.first);
        auto* cb = world.try_get<ColliderComponent>(p.second);
        if (!ta || !tb || !ca || !cb) continue;

        ContactManifold m;
        m.a = p.first; m.b = p.second;
        bool hit = false;

        const auto sa = ca->type, sb = cb->type;
        if      (sa == ShapeType::Sphere && sb == ShapeType::Sphere) hit = contact_sphere_sphere(*ta,*ca, *tb,*cb, m);
        else if (sa == ShapeType::Sphere && sb == ShapeType::Plane)  hit = contact_sphere_plane (*ta,*ca, *tb,*cb, m);
        else if (sa == ShapeType::Plane  && sb == ShapeType::Sphere) { if (contact_sphere_plane(*tb,*cb, *ta,*ca, m)) { std::swap(m.a, m.b); m.normal = m.normal * -1.f; hit = true; } }
        else if (sa == ShapeType::Sphere && sb == ShapeType::Box)    hit = contact_sphere_box   (*ta,*ca, *tb,*cb, m);
        else if (sa == ShapeType::Box    && sb == ShapeType::Sphere) { if (contact_sphere_box(*tb,*cb, *ta,*ca, m)) { std::swap(m.a, m.b); m.normal = m.normal * -1.f; hit = true; } }
        else if (sa == ShapeType::Box    && sb == ShapeType::Plane)  hit = contact_box_plane    (*ta,*ca, *tb,*cb, m);
        else if (sa == ShapeType::Plane  && sb == ShapeType::Box)    { if (contact_box_plane(*tb,*cb, *ta,*ca, m)) { std::swap(m.a, m.b); m.normal = m.normal * -1.f; hit = true; } }
        else if (sa == ShapeType::Box    && sb == ShapeType::Box)    hit = contact_box_box      (*ta,*ca, *tb,*cb, m);

        if (hit) {
            // Combine material parameters multiplicatively (typical mixing).
            m.restitution = ca->restitution * cb->restitution;
            m.friction    = std::sqrt(ca->friction * cb->friction);
            contacts_.push_back(m);
        }
    }
}

void PhysicsWorld::integrate_velocities(ecs::World& world, float dt) {
    world.each<RigidBodyComponent>(
        [&](ecs::Entity, RigidBodyComponent& rb) {
            if (rb.type != BodyType::Dynamic) return;
            // Apply gravity and accumulated forces.
            rb.linear_velocity  += (cfg_.gravity + rb.force * rb.inv_mass) * dt;
            rb.angular_velocity += math::vec3{ rb.torque.x * rb.inv_inertia.x,
                                               rb.torque.y * rb.inv_inertia.y,
                                               rb.torque.z * rb.inv_inertia.z } * dt;
            // Damping.
            rb.linear_velocity  *= (1.f / (1.f + rb.linear_damping  * dt));
            rb.angular_velocity *= (1.f / (1.f + rb.angular_damping * dt));
            rb.force = {0,0,0};
            rb.torque = {0,0,0};
        });
}

void PhysicsWorld::integrate_positions(ecs::World& world, float dt) {
    world.each<TransformComponent, RigidBodyComponent>(
        [&](ecs::Entity, TransformComponent& tc, RigidBodyComponent& rb) {
            if (rb.type == BodyType::Static) return;
            tc.local.position += rb.linear_velocity * dt;
            // Quaternion integration: q' = q + 0.5 * w * q * dt
            const math::vec3 w = rb.angular_velocity;
            math::quat dq{ w.x*0.5f, w.y*0.5f, w.z*0.5f, 0.f };
            math::quat q = tc.local.rotation;
            math::quat r{
                dq.x*q.w + dq.y*q.z - dq.z*q.y + dq.w*q.x,
                -dq.x*q.z + dq.y*q.w + dq.z*q.x + dq.w*q.y,
                 dq.x*q.y - dq.y*q.x + dq.z*q.w + dq.w*q.z,
                -dq.x*q.x - dq.y*q.y - dq.z*q.z + dq.w*q.w,
            };
            q.x += r.x * dt;
            q.y += r.y * dt;
            q.z += r.z * dt;
            q.w += r.w * dt;
            tc.local.rotation = math::normalize(q);
        });
}

void PhysicsWorld::solve(ecs::World& world, float dt) {
    // Sequential impulses with Baumgarte-style positional bias.
    for (int iter = 0; iter < cfg_.solver_iterations; ++iter) {
        for (auto& c : contacts_) {
            auto* ta = world.try_get<TransformComponent>(c.a);
            auto* tb = world.try_get<TransformComponent>(c.b);
            auto* ra = world.try_get<RigidBodyComponent>(c.a);
            auto* rb = world.try_get<RigidBodyComponent>(c.b);
            if (!ta || !tb) continue;
            const float ima = ra ? ra->inv_mass : 0.f;
            const float imb = rb ? rb->inv_mass : 0.f;
            const float inv_mass_sum = ima + imb;
            if (inv_mass_sum <= 0.f) continue;

            const math::vec3 va = ra ? ra->linear_velocity : math::vec3{0,0,0};
            const math::vec3 vb = rb ? rb->linear_velocity : math::vec3{0,0,0};
            const math::vec3 rel = vb - va;
            const float v_n = math::dot(rel, c.normal);

            // Bias term (positional correction).
            const float bias = (cfg_.baumgarte / dt) * std::max(0.f, c.depth - cfg_.penetration_slop);
            // Restitution term (only for incoming velocity above a small threshold).
            const float vrest = (v_n < -1.f) ? -c.restitution * v_n : 0.f;

            float lambda = -(v_n + bias + vrest) / inv_mass_sum;
            // Clamp accumulated impulse to be non-attractive.
            const float new_impulse = std::max(c.normal_impulse + lambda, 0.f);
            lambda = new_impulse - c.normal_impulse;
            c.normal_impulse = new_impulse;

            const math::vec3 P = c.normal * lambda;
            if (ra && ra->type == BodyType::Dynamic) ra->linear_velocity -= P * ima;
            if (rb && rb->type == BodyType::Dynamic) rb->linear_velocity += P * imb;

            // Tangent (friction) impulse. a simple Coulomb model with a
            // single tangent direction (the projected relative velocity).
            const math::vec3 vt = (vb - va) - c.normal * math::dot(vb - va, c.normal);
            const float vt_len = math::length(vt);
            if (vt_len > 1e-5f) {
                const math::vec3 t_dir = vt / vt_len;
                const float jt = -vt_len / inv_mass_sum;
                const float max_friction = c.friction * c.normal_impulse;
                const float clamped = math::clamp(c.tangent_impulse + jt, -max_friction, max_friction);
                const float djt = clamped - c.tangent_impulse;
                c.tangent_impulse = clamped;
                const math::vec3 Pt = t_dir * djt;
                if (ra && ra->type == BodyType::Dynamic) ra->linear_velocity -= Pt * ima;
                if (rb && rb->type == BodyType::Dynamic) rb->linear_velocity += Pt * imb;
            }
        }
    }
}

} // namespace cryogen::physics
