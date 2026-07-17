cmake_minimum_required(VERSION 3.25)

project(material-engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# When building standalone (not as a submodule), find our own dependencies.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")

    find_package(Vulkan REQUIRED)
    find_package(glm CONFIG REQUIRED)
    find_package(SDL3 CONFIG REQUIRED)
    find_package(spdlog CONFIG REQUIRED)
    find_package(imgui CONFIG REQUIRED)
    find_package(VulkanMemoryAllocator CONFIG REQUIRED)
    find_package(OpenXR CONFIG REQUIRED)
endif()

add_subdirectory(glass-ecs)
add_subdirectory(steel)
add_subdirectory(glass)

option(MATERIAL_ENGINE_BUILD_TESTS "Build material-engine tests" OFF)

if(MATERIAL_ENGINE_BUILD_TESTS)
    if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
        find_package(GTest CONFIG REQUIRED)
        enable_testing()
    endif()
    add_subdirectory(test)
endif()
