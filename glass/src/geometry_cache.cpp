#include <glass/geometry_cache.hpp>

namespace glass {

std::shared_ptr<const Geometry> GeometryCache::get(std::string_view key, const Mesh& mesh) {
    auto it = cache_.find(std::string(key));
    if (it != cache_.end()) {
        return it->second;
    }

    auto uploaded = std::make_unique<Geometry>(Geometry::create(engine_, mesh));
    std::shared_ptr<const Geometry> handle(
        uploaded.release(),
        [engine = &engine_](const Geometry* g) {
            engine->defer_destroy(std::unique_ptr<const Geometry>(g));
        });

    cache_.emplace(std::string(key), handle);
    return handle;
}

} // namespace glass
