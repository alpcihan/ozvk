find_package(Vulkan REQUIRED COMPONENTS glslc)
set(GLSLC Vulkan::glslc)

file(GLOB_RECURSE GLSL_SHADERS CONFIGURE_DEPENDS "glsl/*")

if(NOT GLSL_SHADERS)
    message(FATAL_ERROR "No shaders found in ${CMAKE_CURRENT_SOURCE_DIR}/shaders/")
endif()

foreach(GLSL ${GLSL_SHADERS})
    get_filename_component(FILE_NAME ${GLSL} NAME_WE)
    get_filename_component(FILE_EXT ${GLSL} LAST_EXT)
    string(REPLACE "." "" FILE_TYPE ${FILE_EXT})
    set(SPIRV "${FILE_NAME}.${FILE_TYPE}.spv")
    add_custom_command( OUTPUT ${SPIRV}
                        COMMAND ${GLSLC} ${GLSL} -O -o ${SPIRV}
                        DEPENDS ${GLSL})
    list(APPEND SPIRV_SHADERS ${SPIRV})
endforeach(GLSL)

add_custom_target(OZ_SHADERS DEPENDS ${SPIRV_SHADERS})