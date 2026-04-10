#include <glass/camera.hpp>
#include <glass/components.hpp>

#include <gtest/gtest.h>

#include <glm/glm.hpp>

using namespace glass;

// ---- Camera ----

TEST(Camera, Construction) {
    Camera cam(45.0f, 16.0f / 9.0f, 0.1f, 100.0f);
    EXPECT_FLOAT_EQ(cam.near_plane(), 0.1f);
    EXPECT_FLOAT_EQ(cam.far_plane(), 100.0f);
}

TEST(Camera, FovDegreesRoundtrips) {
    Camera cam(60.0f, 1.0f, 0.1f, 100.0f);
    EXPECT_NEAR(cam.fov_degrees(), 60.0f, 0.001f);
}

TEST(Camera, AspectRatioGetter) {
    Camera cam(45.0f, 1.777f, 0.1f, 100.0f);
    EXPECT_FLOAT_EQ(cam.aspect_ratio(), 1.777f);
}

TEST(Camera, ProjectionNotIdentity) {
    Camera cam(45.0f, 1.0f, 0.1f, 100.0f);
    EXPECT_NE(cam.projection(), glm::mat4(1.0f));
}

TEST(Camera, ProjectionHasFocalLengths) {
    Camera cam(45.0f, 1.0f, 0.1f, 100.0f);
    // Diagonal elements encode focal length / aspect
    EXPECT_NE(cam.projection()[0][0], 0.0f);
    EXPECT_NE(cam.projection()[1][1], 0.0f);
}

TEST(Camera, ProjectionHasPerspectiveDivide) {
    Camera cam(45.0f, 1.0f, 0.1f, 100.0f);
    // [2][3] is the perspective divide term in GLM's perspective matrix
    EXPECT_NE(cam.projection()[2][3], 0.0f);
}

TEST(Camera, ProjectionVulkanYFlip) {
    Camera cam(45.0f, 1.0f, 0.1f, 100.0f);
    // Standard OpenGL perspective has positive [1][1], Vulkan flip makes it negative
    EXPECT_LT(cam.projection()[1][1], 0.0f);
}

TEST(Camera, SetAspectRatioUpdatesGetter) {
    Camera cam(45.0f, 1.0f, 0.1f, 100.0f);
    cam.set_aspect_ratio(2.0f);
    EXPECT_FLOAT_EQ(cam.aspect_ratio(), 2.0f);
}

TEST(Camera, SetAspectRatioChangesProjection) {
    Camera cam(45.0f, 1.0f, 0.1f, 100.0f);
    auto proj_before = cam.projection();
    cam.set_aspect_ratio(2.0f);
    EXPECT_NE(cam.projection(), proj_before);
}

TEST(Camera, AspectRatioAffectsHorizontalFocalLength) {
    Camera cam1(45.0f, 1.0f, 0.1f, 100.0f);
    Camera cam2(45.0f, 2.0f, 0.1f, 100.0f);
    // Wider aspect ratio -> smaller [0][0] (horizontal focal length)
    EXPECT_GT(std::abs(cam1.projection()[0][0]), std::abs(cam2.projection()[0][0]));
}

TEST(Camera, AspectRatioDoesNotAffectVerticalFocalLength) {
    Camera cam1(45.0f, 1.0f, 0.1f, 100.0f);
    Camera cam2(45.0f, 2.0f, 0.1f, 100.0f);
    // [1][1] depends only on FOV, not aspect ratio
    EXPECT_FLOAT_EQ(cam1.projection()[1][1], cam2.projection()[1][1]);
}

// ---- Components ----

TEST(Transform, DefaultMatrixIsIdentity) {
    Transform t;
    EXPECT_EQ(t.matrix, glm::mat4(1.0f));
}

TEST(GeometryComponent, DefaultIsNullptr) {
    GeometryComponent gc;
    EXPECT_EQ(gc.geometry, nullptr);
}

TEST(MaterialComponent, DefaultIsNullptr) {
    MaterialComponent mc;
    EXPECT_EQ(mc.material, nullptr);
}

TEST(Velocity, DefaultLinearIsZero) {
    Velocity v;
    EXPECT_EQ(v.linear, glm::vec3(0.0f));
}
