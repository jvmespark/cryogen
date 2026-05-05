//
// cryogen/assets/mesh_loader.cpp
//
#include "cryogen/assets/mesh_loader.hpp"

#include <cmath>
#include <cstring>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "cryogen/core/log.hpp"

namespace cryogen::assets {

namespace {

void update_bounds(render::MeshData& m, math::vec3 p) {
    m.bounds.expand(p);
}

void process_node(const aiScene* scene, const aiNode* node, std::vector<ImportedSubmesh>& out) {
    for (u32 i = 0; i < node->mNumMeshes; ++i) {
        const aiMesh* am = scene->mMeshes[node->mMeshes[i]];
        ImportedSubmesh sub;
        sub.mesh.name = am->mName.length ? am->mName.C_Str() : "submesh";
        sub.mesh.vertices.reserve(am->mNumVertices);
        sub.mesh.indices.reserve(am->mNumFaces * 3);

        for (u32 v = 0; v < am->mNumVertices; ++v) {
            render::Vertex vx{};
            vx.position = { am->mVertices[v].x, am->mVertices[v].y, am->mVertices[v].z };
            if (am->HasNormals()) {
                vx.normal = { am->mNormals[v].x, am->mNormals[v].y, am->mNormals[v].z };
            } else {
                vx.normal = { 0, 1, 0 };
            }
            if (am->HasTangentsAndBitangents()) {
                vx.tangent = { am->mTangents[v].x, am->mTangents[v].y, am->mTangents[v].z };
            }
            if (am->HasTextureCoords(0)) {
                vx.uv = { am->mTextureCoords[0][v].x, am->mTextureCoords[0][v].y };
            }
            sub.mesh.vertices.push_back(vx);
            update_bounds(sub.mesh, vx.position);
        }

        for (u32 f = 0; f < am->mNumFaces; ++f) {
            const aiFace& face = am->mFaces[f];
            if (face.mNumIndices != 3) continue;   // post-process should have triangulated
            sub.mesh.indices.push_back(face.mIndices[0]);
            sub.mesh.indices.push_back(face.mIndices[1]);
            sub.mesh.indices.push_back(face.mIndices[2]);
        }

        // Material params.
        sub.material = render::Material{};
        if (am->mMaterialIndex < scene->mNumMaterials) {
            const aiMaterial* mat = scene->mMaterials[am->mMaterialIndex];
            aiColor4D base{1,1,1,1};
            if (mat->Get(AI_MATKEY_BASE_COLOR, base) == AI_SUCCESS ||
                mat->Get(AI_MATKEY_COLOR_DIFFUSE, base) == AI_SUCCESS) {
                sub.material.base_color = { base.r, base.g, base.b, base.a };
            }
            float v = 0.f;
            if (mat->Get(AI_MATKEY_METALLIC_FACTOR,  v) == AI_SUCCESS) sub.material.metallic  = v;
            if (mat->Get(AI_MATKEY_ROUGHNESS_FACTOR, v) == AI_SUCCESS) sub.material.roughness = v;
            aiColor4D em{0,0,0,0};
            if (mat->Get(AI_MATKEY_COLOR_EMISSIVE, em) == AI_SUCCESS) {
                sub.material.emissive = { em.r, em.g, em.b };
            }
        }

        out.push_back(std::move(sub));
    }

    for (u32 c = 0; c < node->mNumChildren; ++c) {
        process_node(scene, node->mChildren[c], out);
    }
}

} // namespace

std::vector<ImportedSubmesh> load_model_file(const std::string& path) {
    std::vector<ImportedSubmesh> out;

    Assimp::Importer imp;
    const u32 flags = aiProcess_Triangulate
                    | aiProcess_GenSmoothNormals
                    | aiProcess_CalcTangentSpace
                    | aiProcess_JoinIdenticalVertices
                    | aiProcess_ImproveCacheLocality
                    | aiProcess_RemoveRedundantMaterials
                    | aiProcess_FlipUVs;
    const aiScene* scene = imp.ReadFile(path.c_str(), flags);
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        CRYOGEN_ERROR("assets", "Assimp failed to load ", path, ": ", imp.GetErrorString());
        return out;
    }
    process_node(scene, scene->mRootNode, out);
    CRYOGEN_INFO("assets", "Loaded ", out.size(), " submesh(es) from ", path);
    return out;
}

// ---- primitive generators ------------------------------------------------
render::MeshData make_cube(float side) {
    const float h = side * 0.5f;
    render::MeshData m;
    m.name = "cube";
    auto add_face = [&](math::vec3 p0, math::vec3 p1, math::vec3 p2, math::vec3 p3,
                        math::vec3 n) {
        const u32 base = static_cast<u32>(m.vertices.size());
        m.vertices.push_back({p0, n, {1,0,0}, {0,0}});
        m.vertices.push_back({p1, n, {1,0,0}, {1,0}});
        m.vertices.push_back({p2, n, {1,0,0}, {1,1}});
        m.vertices.push_back({p3, n, {1,0,0}, {0,1}});
        m.indices.push_back(base+0); m.indices.push_back(base+1); m.indices.push_back(base+2);
        m.indices.push_back(base+0); m.indices.push_back(base+2); m.indices.push_back(base+3);
        update_bounds(m, p0); update_bounds(m, p1);
        update_bounds(m, p2); update_bounds(m, p3);
    };
    add_face({-h,-h, h},{ h,-h, h},{ h, h, h},{-h, h, h},{0,0,1});  // +Z
    add_face({ h,-h,-h},{-h,-h,-h},{-h, h,-h},{ h, h,-h},{0,0,-1}); // -Z
    add_face({-h, h, h},{ h, h, h},{ h, h,-h},{-h, h,-h},{0,1,0});  // +Y
    add_face({-h,-h,-h},{ h,-h,-h},{ h,-h, h},{-h,-h, h},{0,-1,0}); // -Y
    add_face({ h,-h, h},{ h,-h,-h},{ h, h,-h},{ h, h, h},{1,0,0});  // +X
    add_face({-h,-h,-h},{-h,-h, h},{-h, h, h},{-h, h,-h},{-1,0,0}); // -X
    return m;
}

render::MeshData make_sphere(float r, u32 stacks, u32 slices) {
    render::MeshData m;
    m.name = "sphere";
    for (u32 i = 0; i <= stacks; ++i) {
        const float v  = static_cast<float>(i) / stacks;
        const float ph = v * math::kPi;
        for (u32 j = 0; j <= slices; ++j) {
            const float u  = static_cast<float>(j) / slices;
            const float th = u * math::kTwoPi;
            const float x = std::sin(ph)*std::cos(th);
            const float y = std::cos(ph);
            const float z = std::sin(ph)*std::sin(th);
            render::Vertex vx{};
            vx.position = { x*r, y*r, z*r };
            vx.normal   = { x, y, z };
            vx.tangent  = math::normalize(math::vec3{-std::sin(th), 0.f, std::cos(th)});
            vx.uv       = { u, v };
            m.vertices.push_back(vx);
            update_bounds(m, vx.position);
        }
    }
    const u32 ring = slices + 1;
    for (u32 i = 0; i < stacks; ++i) {
        for (u32 j = 0; j < slices; ++j) {
            const u32 a = i*ring + j;
            const u32 b = a + ring;
            m.indices.push_back(a);   m.indices.push_back(b);   m.indices.push_back(a+1);
            m.indices.push_back(a+1); m.indices.push_back(b);   m.indices.push_back(b+1);
        }
    }
    return m;
}

render::MeshData make_plane(float size) {
    render::MeshData m;
    m.name = "plane";
    const float h = size * 0.5f;
    m.vertices = {
        {{-h,0,-h}, {0,1,0}, {1,0,0}, {0,0}},
        {{ h,0,-h}, {0,1,0}, {1,0,0}, {1,0}},
        {{ h,0, h}, {0,1,0}, {1,0,0}, {1,1}},
        {{-h,0, h}, {0,1,0}, {1,0,0}, {0,1}},
    };
    m.indices = {0,1,2, 0,2,3};
    for (auto& v : m.vertices) update_bounds(m, v.position);
    return m;
}

} // namespace cryogen::assets
