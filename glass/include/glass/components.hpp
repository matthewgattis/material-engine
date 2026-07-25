#pragma once

#include <glass/camera.hpp>
#include <glass/geometry.hpp>
#include <glass/material.hpp>
#include <glass/transform.hpp>

#include <memory>

namespace glass {

// Shared, immutable GPU geometry: many entities may reference one upload.
// Handles from GeometryCache defer GPU destruction until in-flight frames
// retire, so dropping the last reference is safe at any point in a frame.
struct GeometryComponent {
    std::shared_ptr<const Geometry> geometry;
};

struct MaterialComponent {
    const Material* material{nullptr};
};

struct CameraComponent {
    Camera camera;
};

} // namespace glass
