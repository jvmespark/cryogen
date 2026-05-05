//
// cryogen/assets/asset_manager.cpp
//
#include "cryogen/assets/asset_manager.hpp"

#include "cryogen/assets/mesh_loader.hpp"
#include "cryogen/core/log.hpp"

namespace cryogen::assets {

AssetManager::AssetManager(render::VulkanContext& ctx, std::string asset_root)
    : ctx_(ctx), root_(std::move(asset_root)) {}

AssetManager::ModelImport AssetManager::load_model(const std::string& path) {
    ModelImport out;
    const std::string full = root_.empty() ? path : (root_ + "/" + path);
    auto subs = load_model_file(full);
    out.mesh_ids.reserve(subs.size());
    out.material_ids.reserve(subs.size());
    for (size_t i = 0; i < subs.size(); ++i) {
        const std::string mkey = "mesh:"     + path + "#" + std::to_string(i);
        const std::string matkey = "mat:"    + path + "#" + std::to_string(i);
        out.mesh_ids.push_back(register_mesh(std::move(subs[i].mesh), mkey));
        out.material_ids.push_back(register_material(subs[i].material, matkey));
    }
    return out;
}

u32 AssetManager::register_mesh(render::MeshData data, const std::string& key) {
    const u32 id = fnv1a(key);
    if (meshes_.find(id) != meshes_.end()) return id;     // already cached
    auto gpu = std::make_unique<render::Mesh>(ctx_, data);
    mesh_data_.emplace(id, std::move(data));
    meshes_.emplace(id, std::move(gpu));
    return id;
}

u32 AssetManager::register_material(render::Material m, const std::string& key) {
    const u32 id = fnv1a(key);
    materials_[id] = m;
    return id;
}

render::Mesh* AssetManager::mesh(u32 id) {
    auto it = meshes_.find(id);
    return it == meshes_.end() ? nullptr : it->second.get();
}

const render::Mesh* AssetManager::mesh(u32 id) const {
    auto it = meshes_.find(id);
    return it == meshes_.end() ? nullptr : it->second.get();
}

std::optional<render::Material> AssetManager::material(u32 id) const {
    auto it = materials_.find(id);
    if (it == materials_.end()) return std::nullopt;
    return it->second;
}

} // namespace cryogen::assets
