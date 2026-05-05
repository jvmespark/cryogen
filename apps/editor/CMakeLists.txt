# apps/editor/CMakeLists.txt
add_executable(cryogen_editor main.cpp)
target_link_libraries(cryogen_editor PRIVATE cryogen::cryogen)
set_target_properties(cryogen_editor PROPERTIES OUTPUT_NAME cryogen-editor)
