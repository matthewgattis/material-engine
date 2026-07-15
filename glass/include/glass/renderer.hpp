#pragma once

#include <glass/components.hpp>
#include <glass/world.hpp>
#include <steel/engine.hpp>
#include <steel/uniform_buffer.hpp>
#include <steel/xr_system.hpp>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <array>
#include <memory>

namespace glass {

struct FrameUBO {
    glm::mat4 view;
    glm::mat4 projection;
};

class Renderer {
public:
    explicit Renderer(steel::Engine& engine);

    void bind_world(World& world);

    void set_camera(Entity camera) { camera_ = camera; }
    Entity camera() const { return camera_; }

    void run(World& world);
    void render_frame(World& world);

    const vk::raii::DescriptorSetLayout& frame_descriptor_layout() const { return frame_ubo_.layout(); }

    // XR management
    void init_xr(World& world);
    void shutdown_xr();
    bool xr_active() const { return xr_system_ && xr_system_->active(); }
    steel::XrSystem* xr_system() const { return xr_system_.get(); }
    Entity xr_head() const { return xr_head_; }

private:
    void render_xr_eyes(const vk::raii::CommandBuffer& cmd,
                        World& world,
                        uint32_t frame_index,
                        steel::XrFrameState& frame_state);

    void render_desktop_companion(const vk::raii::CommandBuffer& cmd,
                                  World& world,
                                  uint32_t frame_index,
                                  const glm::mat4* xr_view = nullptr);

    void render_ecs(const vk::raii::CommandBuffer& cmd,
                    World& world,
                    uint32_t frame_index,
                    const steel::UniformBuffer<FrameUBO>& ubo) const;

    steel::Engine& engine_;
    steel::UniformBuffer<FrameUBO> frame_ubo_;
    std::array<steel::UniformBuffer<FrameUBO>, 2> xr_eye_ubos_;
    Entity camera_{null_entity};

    // XR state
    std::unique_ptr<steel::XrSystem> xr_system_;
    Entity xr_head_{null_entity};
    World* world_{nullptr};
};

} // namespace glass
