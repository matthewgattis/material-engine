#include <glass/renderer.hpp>

#include <glass/hierarchy.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>

namespace glass {

Renderer::Renderer(steel::Engine& engine)
    : engine_(engine)
    , frame_ubo_{steel::UniformBuffer<FrameUBO>::create(
          engine_, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)}
    , xr_eye_ubos_{
          steel::UniformBuffer<FrameUBO>::create(
              engine_, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment),
          steel::UniformBuffer<FrameUBO>::create(
              engine_, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)} {}

void Renderer::bind_world(World& world) {
    // GPU cleanup needs no subscription here: GeometryComponent handles carry
    // a deferred-destroy deleter (see GeometryCache), so releasing the last
    // reference is safe on any path.
    world_ = &world;
}

void Renderer::run(World& world) {
    while (engine_.poll_events()) {
        render_frame(world);
    }
    engine_.wait_idle();
}

// ---------------------------------------------------------------------------
// XR management
// ---------------------------------------------------------------------------

void Renderer::init_xr(World& world) {
    if (!steel::XrSystem::has_pending_session()) return;

    xr_system_ = std::make_unique<steel::XrSystem>(
        static_cast<VkInstance>(*engine_.instance()),
        static_cast<VkPhysicalDevice>(*engine_.physical_device()),
        static_cast<VkDevice>(*engine_.device()),
        engine_.graphics_family(), 0,
        engine_.allocator(),
        engine_.color_format(), engine_.depth_format(),
        engine_.device());

    // Create head entity as child of camera (body)
    if (camera_ != null_entity) {
        xr_head_ = world.create();
        world.add<Transform>(xr_head_, Transform{});
        world.add<Parent>(xr_head_, Parent{camera_});
    }
}

void Renderer::shutdown_xr() {
    xr_system_.reset();
}

// ---------------------------------------------------------------------------
// Unified render
// ---------------------------------------------------------------------------

void Renderer::render_frame(World& world) {
    if (camera_ == null_entity || !world.alive(camera_)) return;
    if (!world.has<Transform>(camera_) || !world.has<CameraComponent>(camera_)) return;

    if (xr_system_) xr_system_->poll_events();

    auto& cc = world.get<CameraComponent>(camera_);

    auto extent = engine_.extent();
    float aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
    cc.camera.set_aspect_ratio(aspect);

    if (xr_system_ && xr_system_->active()) {
        // Extract body position and yaw from camera entity's transform
        auto& body_t = world.get<Transform>(camera_);
        glm::vec3 body_pos = glm::vec3(body_t.matrix[3]);

        // Extract yaw: forward is column 1 (Y-axis) of the rotation matrix
        glm::vec3 forward_xy = glm::vec3(body_t.matrix[1]);
        float body_yaw = std::atan2(-forward_xy.x, forward_xy.y);

        auto xr_state = xr_system_->wait_and_begin_frame(body_pos, body_yaw);

        // Update head entity's local transform from headset pose
        if (xr_state.should_render && xr_head_ != null_entity) {
            // The XR eye view is world-space; extract head-local transform
            // by removing the body transform: head_local = inverse(body_world) * head_world
            glm::mat4 head_world = glm::inverse(xr_state.eyes[0].view);
            glm::mat4 body_world = world_transform(world, camera_);
            glm::mat4 head_local = glm::inverse(body_world) * head_world;
            world.get<Transform>(xr_head_).matrix = head_local;
        }

        auto* cmd = engine_.begin_command_buffer();
        if (cmd) {
            if (xr_state.should_render) {
                render_xr_eyes(*cmd, world, engine_.current_frame(), xr_state);
            }

            engine_.begin_scene_pass();
            const glm::mat4* mirror_view =
                xr_state.should_render ? &xr_state.eyes[0].view : nullptr;
            render_desktop_companion(*cmd, world, engine_.current_frame(), mirror_view);
            engine_.end_frame();
        }

        xr_system_->end_frame(xr_state);
    } else {
        // Desktop path
        glm::mat4 view = glm::inverse(world_transform(world, camera_));

        auto* cmd = engine_.begin_frame();
        if (cmd) {
            frame_ubo_.update(engine_.current_frame(), FrameUBO{view, cc.camera.projection()});
            render_ecs(*cmd, world, engine_.current_frame(), frame_ubo_);
            engine_.end_frame();
        }
    }
}

// ---------------------------------------------------------------------------
// XR eye rendering
// ---------------------------------------------------------------------------

void Renderer::render_xr_eyes(const vk::raii::CommandBuffer& cmd,
                              World& world,
                              uint32_t frame_index,
                              steel::XrFrameState& frame_state) {
    for (uint32_t eye = 0; eye < 2; ++eye) {
        xr_eye_ubos_[eye].update(frame_index,
                                 FrameUBO{frame_state.eyes[eye].view,
                                          frame_state.eyes[eye].projection});

        xr_system_->begin_eye_render(cmd, eye);
        render_ecs(cmd, world, frame_index, xr_eye_ubos_[eye]);
        xr_system_->end_eye_render(cmd, eye);
    }
}

// ---------------------------------------------------------------------------
// Desktop companion
// ---------------------------------------------------------------------------

void Renderer::render_desktop_companion(const vk::raii::CommandBuffer& cmd,
                                        World& world,
                                        uint32_t frame_index,
                                        const glm::mat4* xr_view) {
    if (camera_ == null_entity || !world.alive(camera_)) return;
    if (!world.has<Transform>(camera_) || !world.has<CameraComponent>(camera_)) return;

    auto& cc = world.get<CameraComponent>(camera_);

    glm::mat4 view = xr_view ? *xr_view : glm::inverse(world_transform(world, camera_));
    frame_ubo_.update(frame_index, FrameUBO{view, cc.camera.projection()});
    render_ecs(cmd, world, frame_index, frame_ubo_);
}

// ---------------------------------------------------------------------------
// Core ECS rendering
// ---------------------------------------------------------------------------

void Renderer::render_ecs(const vk::raii::CommandBuffer& cmd,
                          World& world,
                          uint32_t frame_index,
                          const steel::UniformBuffer<FrameUBO>& ubo) const {
    world.view<Transform, GeometryComponent, MaterialComponent>()
        .each([&](Entity e, Transform& t, GeometryComponent& mesh, MaterialComponent& mat) {
            mat.material->bind(cmd);
            ubo.bind(cmd, mat.material->layout(), 0, frame_index);
            cmd.pushConstants<glm::mat4>(
                *mat.material->layout(),
                vk::ShaderStageFlagBits::eVertex,
                0,
                world_transform(world, e));
            mesh.geometry->bind(cmd);
            mesh.geometry->draw(cmd);
        });
}

} // namespace glass
