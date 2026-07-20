#pragma once

#include <glass/geometry.hpp>
#include <glass/mesh.hpp>
#include <steel/engine.hpp>

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace glass {

// GPU-side geometry cache: uploads a mesh once per key and hands out shared
// handles, so any number of entities render from the same vertex buffers.
// Each handle carries a deleter that routes the geometry through the engine's
// deferred-destruction queue, so the GPU buffers are freed only after
// in-flight frames retire — whoever drops the last reference, whenever.
// Handles must not outlive the engine.
class GeometryCache {
public:
    explicit GeometryCache(steel::Engine& engine) : engine_(engine) {}

    // Returns the cached geometry for key, uploading mesh on first request.
    std::shared_ptr<const Geometry> get(std::string_view key, const Mesh& mesh);

    // Drops the cache's reference (e.g. before re-uploading a hot-reloaded
    // asset). Outstanding handles keep the old geometry alive until released.
    void erase(std::string_view key) { cache_.erase(std::string(key)); }

    size_t size() const { return cache_.size(); }

private:
    steel::Engine& engine_;
    std::unordered_map<std::string, std::shared_ptr<const Geometry>> cache_;
};

} // namespace glass
