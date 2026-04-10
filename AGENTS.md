# AGENTS.md

## Project Overview

Material Engine is a reusable Vulkan engine library using C++23 with CMake and vcpkg. Two-layer architecture: `steel` (Vulkan RAII wrappers + OpenXR) -> `glass` (engine abstractions with ECS, event dispatch, rendering). Designed to be consumed as a git submodule by application projects. Always-on FXAA 3.11 anti-aliasing and Dear ImGui debug overlay are applied as post-processing passes on the desktop view inside `steel::Engine`.

## Build

```bash
cmake --preset default
cmake --build build
cd build && ctest --output-on-failure
```

Requires: CMake 3.25+, C++23 compiler, Vulkan-capable GPU. All dependencies (including `glslc` via shaderc) managed by vcpkg.

### As a Submodule

```cmake
# Parent project finds packages, then:
add_subdirectory(material-engine)
target_link_libraries(my_app PRIVATE glass)
```

When added via `add_subdirectory()`, the parent project is responsible for calling `find_package()` for all required dependencies. Material Engine only calls `find_package()` when built standalone.

## Code Organization

- **Top-level CMakeLists.txt**: Finds packages (standalone only) and adds subdirectories. Do not add targets here.
- **steel/**: Vulkan RAII engine library. Namespace `steel`. Links against Vulkan, GLM, SDL3, spdlog, imgui, VMA, OpenXR.
- **glass/**: Engine abstraction layer. Namespace `glass`. Links against `steel`. Provides meshes, materials, ECS (Entity Component System), event dispatch, and rendering abstractions built on top of steel's Vulkan wrappers.
- **test/**: Google Test suite. Links against `steel`, `glass`, and GTest. Built when `MATERIAL_ENGINE_BUILD_TESTS=ON`.

Each subdirectory has its own `CMakeLists.txt`.

## Conventions

- **C++ standard**: C++23, no extensions
- **Namespaces**: `steel` for Vulkan RAII wrappers, `glass` for engine abstractions
- **Vulkan**: Use `vk::raii::` types exclusively (RAII wrappers, no manual cleanup)
- **Headers**: `<module>/include/<module>/` layout (e.g., `steel/include/steel/engine.hpp`)
- **Shaders**: GLSL 450 in `steel/shaders/`, compiled to SPIR-V by `glslc` at build time. Steel's internal FXAA shaders (`fullscreen.vert`, `fxaa.frag`) are compiled to SPIR-V and embedded as `constexpr` arrays in a generated header (not checked into git).
- **Front face**: Default front face is clockwise (`vk::FrontFace::eClockwise`)
- **Push constants**: Used for per-object model transforms, pushed per draw call
- **Descriptor sets**: Set 0 = per-frame UBO (view + projection matrices), set 1 = reserved for per-material (future)
- **Tests**: No GPU required. Test struct layouts, type traits, Vulkan struct construction, and utilities.
- **Vulkan HPP structs**: Use member assignment or constructor syntax, not C++20 designated initializers (they do not work reliably with Vulkan HPP types)

## Key Interfaces

### steel::Engine
- `Engine(title)` or `Engine(EngineConfig)` — creates window and initializes Vulkan. Auto-selects largest fitting 4:3 resolution from predefined list for the primary display. `EngineConfig` supports extra Vulkan instance/device extensions, API version override, and a physical device query callback (used by OpenXR).
- High-DPI support via `SDL_WINDOW_HIGH_PIXEL_DENSITY`
- `begin_frame()` -> `const vk::raii::CommandBuffer*` (nullptr if frame unavailable). Calls `begin_command_buffer()` + `begin_scene_pass()`. Sets dynamic viewport and scissor from the current extent. Flushes deferred destruction queue.
- `begin_command_buffer()` -> `const vk::raii::CommandBuffer*` — fence wait, swapchain acquire, begin command buffer (without starting scene render pass). Used by XR path.
- `begin_scene_pass()` — begins the offscreen scene render pass. Called separately in XR mode after XR eye rendering.
- `end_frame()` — submits and presents
- FXAA 3.11 post-processing: the scene renders to an offscreen target, then an FXAA fullscreen pass (quality preset 12 with edge endpoint search) reads it via a combined image sampler descriptor and writes to the swapchain. The FXAA pipeline is built directly, separate from `PipelineBuilder`. The `begin_frame()`/`end_frame()` API is unchanged — consumers are unaware of FXAA.
- `wait_idle()` — waits for device idle (used for clean shutdown)
- `poll_events()` -> `bool` (false = quit requested). Handles quit, resize, and delta time. Forwards all other events via optional event callback.
- `set_event_callback(fn)` — single event callback slot, typically claimed by `glass::EventDispatcher`
- `delta_time()` — frame delta in seconds, clamped to 0.1s max
- `current_frame()` — current frame-in-flight index
- `defer_destroy<T>(resource)` — type-erased deferred destruction, holds resource for `MAX_FRAMES_IN_FLIGHT + 1` frames
- `window()` -> `SDL_Window*`
- ImGui: `imgui_begin()`, `imgui_end()`, `imgui_enabled()`, `set_imgui_enabled()`, `imgui_process_event()`
- Frames in flight: `MAX_FRAMES_IN_FLIGHT` (2, defined in engine.hpp)
- Accessors: `instance()`, `device()`, `physical_device()`, `render_pass()`, `extent()`, `command_pool()`, `graphics_queue()`, `graphics_family()`, `color_format()`, `depth_format()`, `allocator()`

### steel::UniformBuffer\<T\>
- Header-only template encapsulating descriptor set layout, pool, per-frame-in-flight sets, buffers, and persistent mapping
- `create(engine, stages)` — creates layout, pool, sets, buffers with persistent mapping
- `update(frame_index, data)` — memcpy to mapped buffer
- `bind(cmd, layout, set_index, frame_index)` — binds descriptor set
- `layout()` -> `const vk::raii::DescriptorSetLayout&`

### steel::PipelineBuilder
- Constructor: `PipelineBuilder(device, vert_spirv, frag_spirv)` — takes SPIR-V bytecode upfront
- Fluent API for remaining state: `set_vertex_input(bindings, attrs)`, `set_topology()`, `set_polygon_mode()`, `set_cull_mode()`, `set_depth_test()`
- Default front face is `eClockwise` (matching Vulkan convention with Y-flipped projection)
- `build(render_pass, layout)` -> `vk::raii::Pipeline` — viewport and scissor are dynamic state

### steel::Buffer
- `Buffer::create_vertex_buffer(...)` — staging upload to device-local vertex buffer
- `Buffer::create_index_buffer(...)` — staging upload to device-local index buffer
- `Buffer::create(...)` — general buffer creation
- `map()`, `unmap()` — host-visible memory access

### steel::XrSystem
- OpenXR integration for HMD stereo rendering via `XR_KHR_vulkan_enable`
- Static two-phase initialization: `query_requirements()` runs before Vulkan setup (returns required extensions + physical device query), constructor runs after `VkDevice` creation
- `query_requirements()` -> `optional<XrVulkanRequirements>` — creates XrInstance, queries HMD system, returns required Vulkan extensions. Returns nullopt if no HMD. Stores static XR state for constructor.
- `query_physical_device(VkInstance)` -> `VkPhysicalDevice` — queries the GPU the XR runtime requires
- `has_pending_session()` — true if `query_requirements()` found an HMD
- Constructor takes Vulkan handles, creates XrSession, reference space (LOCAL/seated), per-eye swapchains, depth buffers, render pass, and framebuffers
- `poll_events()` — handles session state transitions
- `active()` — true when session is running
- `wait_and_begin_frame(body_position, body_yaw)` -> `XrFrameState` — xrWaitFrame, xrBeginFrame, xrLocateViews
- `begin_eye_render(cmd, eye)` / `end_eye_render(cmd, eye)` — per-eye render pass management
- `end_frame(XrFrameState)` — xrEndFrame with projection layer
- Coordinate transform: OpenXR Y-up -> engine Z-up via -90 deg X rotation

### glass::EventDispatcher
- `EventDispatcher(engine)` — registers as `steel::Engine`'s sole event callback, fans out SDL events to multiple subscribers
- `subscribe(callback)` -> `Subscription` — RAII subscription handle; dropping it unsubscribes automatically
- Callback signature: `void(const SDL_Event&, bool& handled)`

### glass::Camera
- Projection-only: `Camera(fov_degrees, aspect_ratio, near_plane, far_plane)`
- `set_aspect_ratio(float)` — updated by renderer each frame
- `projection()` -> `const glm::mat4&` — Y-flipped for Vulkan
- View matrix is derived from `glm::inverse(Transform.matrix)` in the renderer

### glass::Renderer
- `Renderer(engine)` — creates per-frame UBO with separate view and projection matrices
- `bind_world(world)` — registers pre-destroy callback for automatic GPU resource cleanup
- `set_camera(entity)` — sets the active camera entity
- `render_frame(world)` — desktop rendering path
- `render_xr_eyes(cmd, world, frame_index, frame_state, xr)` — stereo XR rendering
- `render_desktop_companion(cmd, world, frame_index, xr_view)` — desktop companion view for XR mode
- `frame_descriptor_layout()` — exposes UBO descriptor set layout for material pipeline creation
- `FrameUBO` — `mat4 view` + `mat4 projection`

### glass::Components
- `Transform` — `glm::mat4 matrix` (default identity)
- `GeometryComponent` — `std::unique_ptr<Geometry>` (owns GPU buffers)
- `MaterialComponent` — `const Material*` (non-owning)
- `Velocity` — `glm::vec3 linear` (default zero)
- `CameraComponent` — `Camera camera`

### glass::Entity, World, View
- `Entity` — lightweight handle: `uint32_t index` + `uint32_t generation`
- `World` — entity manager with create/destroy, component operations (`add`, `remove`, `get`, `has`), `view<Ts...>()` for multi-component queries. Supports `set_on_destroy(callback)`.
- `View<Ts...>` — iterates smallest pool, filters by all requested types, `each(fn)` callback

### glass::Material
- `Material::create(engine, vertex_shader, fragment_shader, frame_descriptor_layout)` — pipeline layout includes descriptor set layout at set 0 and push constant range for model matrix
- `bind(cmd)`, `layout()` — pipeline binding and layout access

### glass::Mesh, Geometry, Shader, Vertex
- `Mesh` — abstract interface: `vertices()`, `indices()`
- `Geometry::create(engine, mesh)` — uploads mesh to GPU via staging buffers. `bind(cmd)`, `draw(cmd)`.
- `Shader::load(stage, spirv_path)` — loads SPIR-V from file
- `Vertex` — `vec3 position` + `vec3 normal` + `vec3 color` (36 bytes)

## Adding New Code

- New steel features: add files under `steel/src/` and `steel/include/steel/`, update `steel/CMakeLists.txt`
- New glass features: add files under `glass/src/` and `glass/include/glass/`, update `glass/CMakeLists.txt`
- New tests: add `.cpp` files under `test/`, update `test/CMakeLists.txt`
- New engine shaders: add `.vert`/`.frag` under `steel/shaders/`, update `steel/CMakeLists.txt` to compile and embed them
- New dependencies: add to `vcpkg.json`, `find_package()` in top-level CMakeLists.txt (guarded by standalone check), link in the appropriate subdirectory. **Parent projects consuming material-engine as a submodule must also add the dependency to their own `vcpkg.json`** — material-engine's manifest is only used for standalone builds.
