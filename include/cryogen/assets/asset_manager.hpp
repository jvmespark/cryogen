#pragma once
//
// cryogen/assets/asset_manager.hpp
// A tiny content-addressed resource cache for meshes and materials.
// Resources are referenced by 32-bit ID = FNV-1a hash of "<kind>:<path>".
// The renderer holds GPU handles indexed by this same id, so by-ID is enough
// to find both a Mesh* and its source MeshData.
//

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

#include "cryogen/core/types.hpp"
#include "cryogen/render/material.hpp"
#include "cryogen/render/mesh.hpp"

namespace cryogen::render { class VulkanContext; }

namespace cryogen::assets {

// Compile-time-friendly FNV-1a (32-bit).
inline u32 fnv1a(const std::string& s) {
    u32 h = 0x811c9dc5u;
    for (unsigned char c : s) { h ^= c; h *= 0x01000193u; }
    return h;
}

class AssetManager {
public:
    explicit AssetManager(render::VulkanContext& ctx, std::string asset_root = "assets");

    // Load (or fetch from cache) an entire model file. Returns the mesh ID(s)
    // for each submesh in load order, plus matching material IDs.
    struct ModelImport {
        std::vector<u32> mesh_ids;
        std::vector<u32> material_ids;
    };
    ModelImport load_model(const std::string& path);

    // Direct registration (used by code that builds geometry programmatically,
    // e.g. tests and the editor's primitive insertion).
    u32 register_mesh    (render::MeshData data, const std::string& key);
    u32 register_material(render::Material  m,    const std::string& key);

    render::Mesh*       mesh(u32 id);
    const render::Mesh* mesh(u32 id) const;
    std::optional<render::Material> material(u32 id) const;

    const std::unordered_map<u32, std::unique_ptr<render::Mesh>>& meshes() const { return meshes_; }
    const std::unordered_map<u32, render::Material>&             materials() const { return materials_; }

private:
    render::VulkanContext& ctx_;
    std::string            root_;
    std::unordered_map<u32, std::unique_ptr<render::Mesh>>  meshes_;
    std::unordered_map<u32, render::Material>               materials_;
    std::unordered_map<u32, render::MeshData>               mesh_data_; // CPU-side copy for AS builds & tests
};

} // namespace cryogen::assets
