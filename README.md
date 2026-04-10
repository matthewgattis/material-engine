# Material Engine

Reusable Vulkan engine library built with C++23. Two-layer architecture:

- **steel** — Low-level Vulkan RAII wrappers: window/device initialization, swapchain management, pipeline builder, buffer uploads, uniform buffer templates, FXAA post-processing, Dear ImGui integration, and OpenXR HMD support.
- **glass** — Engine abstractions: Entity Component System (sparse-set ECS with World/Entity/View), event dispatch, perspective camera, material/shader/mesh/geometry system, and a renderer supporting both desktop and stereo XR rendering.

## Build

Requires CMake 3.25+, a C++23 compiler, and a Vulkan-capable GPU. All dependencies are managed by vcpkg.

```bash
git clone --recursive <repo-url>
cmake --preset default
cmake --build build
cd build && ctest --output-on-failure
```

## Use as a Submodule

Add material-engine as a git submodule in your project:

```bash
git submodule add <repo-url> material-engine
git submodule update --init --recursive
```

In your root `CMakeLists.txt`, find the required packages and then add the subdirectory:

```cmake
find_package(Vulkan REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(OpenXR CONFIG REQUIRED)

add_subdirectory(material-engine)

target_link_libraries(my_app PRIVATE glass)
```

Linking against `glass` transitively brings in `steel` and all of its dependencies.

## Dependencies

| Library | Purpose |
|---|---|
| Vulkan | Graphics API |
| VulkanMemoryAllocator | GPU memory management |
| GLM | Linear algebra |
| SDL3 | Windowing and input |
| spdlog | Logging |
| Dear ImGui | Debug UI overlay |
| OpenXR | VR headset support |
| shaderc | GLSL to SPIR-V compilation (build-time) |
