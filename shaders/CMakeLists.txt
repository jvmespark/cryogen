# shaders/CMakeLists.txt
# Compiles every .vert / .frag / .comp / .rgen / .rmiss / .rchit file in this
# directory to SPIR-V using glslangValidator (shipped with the Vulkan SDK).

find_program(GLSLANG_VALIDATOR
    NAMES glslangValidator glslangValidator.exe
    HINTS "$ENV{VULKAN_SDK}/bin" "$ENV{VULKAN_SDK}/Bin"
)

if(NOT GLSLANG_VALIDATOR)
    message(WARNING "glslangValidator not found; shader compilation will be skipped.")
    return()
endif()

set(SHADER_OUT_DIR "${CMAKE_BINARY_DIR}/shaders")
file(MAKE_DIRECTORY "${SHADER_OUT_DIR}")

file(GLOB SHADER_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.vert"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.frag"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.comp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.rgen"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.rmiss"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.rchit"
)

set(SHADER_SPIRVS)
foreach(SHADER IN LISTS SHADER_SOURCES)
    get_filename_component(NAME ${SHADER} NAME)
    set(OUT "${SHADER_OUT_DIR}/${NAME}.spv")
    add_custom_command(
        OUTPUT  ${OUT}
        COMMAND ${GLSLANG_VALIDATOR} --target-env vulkan1.2 -V ${SHADER} -o ${OUT}
        DEPENDS ${SHADER}
        COMMENT "GLSL -> SPIR-V : ${NAME}"
        VERBATIM
    )
    list(APPEND SHADER_SPIRVS ${OUT})
endforeach()

add_custom_target(cryogen_shaders ALL DEPENDS ${SHADER_SPIRVS})
