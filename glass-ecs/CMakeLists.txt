add_library(glass-ecs)

target_sources(glass-ecs PRIVATE
    src/world.cpp
)

target_include_directories(glass-ecs PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(glass-ecs PUBLIC
    glm::glm
)
