find_package(Vulkan REQUIRED)

file(GLOB_RECURSE OZ_SOURCES CONFIGURE_DEPENDS "oz/*.cpp")
add_library(oz ${OZ_SOURCES})

target_include_directories(oz
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(oz
    PUBLIC
        Vulkan::Vulkan
        glfw
        glm
)

add_dependencies(oz OZ_SHADERS)

target_compile_definitions(oz PRIVATE
    SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    BUILD_DIR="${CMAKE_BINARY_DIR}"
)
