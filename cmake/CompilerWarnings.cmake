# cmake/CompilerWarnings.cmake
# Centralised warning configuration applied via target_compile_options on the
# `cryogen_warnings` interface target.

add_library(cryogen_warnings INTERFACE)

if(MSVC)
    target_compile_options(cryogen_warnings INTERFACE
        /W4 /permissive- /Zc:__cplusplus /Zc:preprocessor /utf-8 /MP
        /wd4324  # structure padded due to alignment specifier
    )
    target_compile_definitions(cryogen_warnings INTERFACE
        _CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
else()
    target_compile_options(cryogen_warnings INTERFACE
        -Wall -Wextra -Wpedantic
        -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-align
        -Wunused -Woverloaded-virtual -Wconversion -Wsign-conversion
        -Wnull-dereference -Wdouble-promotion -Wformat=2
        -Wno-unused-parameter
    )
endif()
