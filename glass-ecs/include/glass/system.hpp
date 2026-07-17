#pragma once

#include <array>
#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

namespace glass {

class World;

// Well-defined points in a frame/tick at which systems run. Sim phases
// (PreTick/Tick/PostTick) advance game state at a fixed rate; render phases
// run per displayed frame and should treat the world as read-mostly.
enum class Phase : uint8_t {
    PreTick,
    Tick,
    PostTick,
    PreRender,
    Render,
    Count,
};

// Registry of systems. A system is any callable (World&, float dt); stateful
// systems register member functions bound to a long-lived object, one per
// phase they care about. Within a phase, systems run in registration order.
class Scheduler {
public:
    using SystemFn = std::function<void(World&, float)>;

    void add(Phase phase, SystemFn fn) {
        systems_[static_cast<size_t>(phase)].push_back(std::move(fn));
    }

    void run(Phase phase, World& world, float dt) {
        for (auto& fn : systems_[static_cast<size_t>(phase)]) {
            fn(world, dt);
        }
    }

private:
    std::array<std::vector<SystemFn>, static_cast<size_t>(Phase::Count)> systems_;
};

} // namespace glass
