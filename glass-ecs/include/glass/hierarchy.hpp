#pragma once

#include <glass/transform.hpp>
#include <glass/world.hpp>

#include <glm/mat4x4.hpp>

namespace glass {

// Compute world-space transform by walking the parent chain. Depth-capped so
// a malformed Parent cycle yields a truncated transform instead of hanging
// the caller.
inline glm::mat4 world_transform(World& world, Entity e) {
    glm::mat4 result = world.get<Transform>(e).matrix;
    for (int depth = 0; depth < 64 && world.has<Parent>(e); ++depth) {
        e = world.get<Parent>(e).entity;
        result = world.get<Transform>(e).matrix * result;
    }
    return result;
}

} // namespace glass
