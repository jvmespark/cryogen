#pragma once
//
// cryogen/scene/serializer.hpp
// JSON-backed scene save/load. Format is hand-written and human-readable; the
// editor uses it for save-as, the runtime app uses it on startup, and the
// integration tests write/read deterministic snapshots.
//
//   {
//     "version": 1,
//     "camera": { "position":[..], "yaw":.., "pitch":.. },
//     "sun":    { "direction":[..], "color":[..], "intensity":.. },
//     "entities": [
//       { "name":"Cube", "transform":{...}, "mesh":{...},
//         "collider":{...}, "rigid_body":{...} }
//     ]
//   }
//

#include <string>

namespace cryogen::assets { class AssetManager; }

namespace cryogen::scene {

class Scene;

struct SceneIO {
    static bool save(const Scene& scene, const std::string& path);
    static bool load(Scene& scene, assets::AssetManager& am, const std::string& path);
};

} // namespace cryogen::scene
