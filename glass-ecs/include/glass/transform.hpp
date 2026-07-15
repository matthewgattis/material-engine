#pragma once

#include <glass/entity.hpp>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace glass {

struct Transform {
    glm::mat4 matrix{1.0f};
};

struct Velocity {
    glm::vec3 linear{0.0f};
};

struct Parent {
    Entity entity{null_entity};
};

} // namespace glass
