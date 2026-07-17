#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace glass {

struct Transform {
    glm::mat4 matrix{1.0f};
};

struct Velocity {
    glm::vec3 linear{0.0f};
};

} // namespace glass
