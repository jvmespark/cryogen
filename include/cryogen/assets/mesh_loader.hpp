#pragma once
//
// cryogen/assets/mesh_loader.hpp
// Assimp-backed mesh loader. Returns CPU-side MeshData (vertices + indices +
// AABB) for one model file. PBR material parameters are read from any
// AI_MATKEY_BASE_COLOR / AI_MATKEY_METALLIC_FACTOR / AI_MATKEY_ROUGHNESS_FACTOR
// the file exposes; falls back to legacy diffuse otherwise.
//

#include <string>
#include <vector>

#include "cryogen/render/material.hpp"
#include "cryogen/render/mesh.hpp"

namespace cryogen::assets {

struct ImportedSubmesh {
    render::MeshData mesh;
    render::Material material;
};

// Load all submeshes from a model file (formats supported by Assimp).
// Coordinates are converted to a left-Y-up right-handed system and indices
// are flattened to uint32. Returns an empty vector on failure (logged).
std::vector<ImportedSubmesh> load_model_file(const std::string& path);

// Built-in primitive generators. used by the editor's "Insert primitive"
// action and by tests. Bounds are populated.
render::MeshData make_cube      (float side = 1.f);
render::MeshData make_sphere    (float radius = 0.5f, u32 stacks = 24, u32 slices = 32);
render::MeshData make_plane     (float size = 10.f);

} // namespace cryogen::assets
