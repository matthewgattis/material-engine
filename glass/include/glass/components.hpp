#pragma once

#include <glass/camera.hpp>
#include <glass/geometry.hpp>
#include <glass/material.hpp>
#include <glass/transform.hpp>

#include <memory>

namespace glass {

struct GeometryComponent {
    std::unique_ptr<Geometry> geometry;
};

struct MaterialComponent {
    const Material* material{nullptr};
};

struct CameraComponent {
    Camera camera;
};

} // namespace glass
