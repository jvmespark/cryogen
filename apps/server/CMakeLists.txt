# apps/server/CMakeLists.txt
add_executable(cryogen_server main.cpp)
target_link_libraries(cryogen_server PRIVATE cryogen::cryogen)
set_target_properties(cryogen_server PROPERTIES OUTPUT_NAME cryogen-server)
