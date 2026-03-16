set(OZ_LIB_NAME "oz" CACHE STRING "oz Library Name")

find_package(Vulkan REQUIRED)

file(GLOB_RECURSE OZ_LIB_SOURCES CONFIGURE_DEPENDS "./*.cpp")
add_library(${OZ_LIB_NAME} ${OZ_LIB_SOURCES})

target_include_directories(${OZ_LIB_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${OZ_LIB_NAME} 
    PUBLIC
        Vulkan::Vulkan
        glfw
        glm
)

add_dependencies(${OZ_LIB_NAME} OZ_SHADERS)

add_executable(main main.cpp)
target_link_libraries(main ${OZ_LIB_NAME})

# Set rpath so the binary finds Vulkan libraries at runtime
if(APPLE)
    get_filename_component(_vulkan_lib_dir "${Vulkan_LIBRARIES}" DIRECTORY)
    set_target_properties(main PROPERTIES
        BUILD_RPATH "${_vulkan_lib_dir}"
        INSTALL_RPATH "${_vulkan_lib_dir}"
    )
endif()

target_compile_definitions(${OZ_LIB_NAME} PRIVATE
    SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    BUILD_DIR="${CMAKE_BINARY_DIR}"
)