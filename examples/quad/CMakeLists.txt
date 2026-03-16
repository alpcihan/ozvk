add_executable(quad main.cpp)
target_link_libraries(quad oz)

if(APPLE)
    get_filename_component(_vulkan_lib_dir "${Vulkan_LIBRARIES}" DIRECTORY)
    set_target_properties(quad PROPERTIES
        BUILD_RPATH "${_vulkan_lib_dir}"
        INSTALL_RPATH "${_vulkan_lib_dir}"
    )
endif()
