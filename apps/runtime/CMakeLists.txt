# apps/runtime/CMakeLists.txt
add_executable(cryogen_runtime main.cpp)
target_link_libraries(cryogen_runtime PRIVATE cryogen::cryogen)
set_target_properties(cryogen_runtime PROPERTIES OUTPUT_NAME cryogen-runtime)
