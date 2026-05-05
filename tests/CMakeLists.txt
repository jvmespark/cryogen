# tests/CMakeLists.txt
# A header-only "test framework": a CRYOGEN_CHECK macro that aborts with a
# diagnostic on failure. Each test is its own executable so they run in
# isolation under ctest.

add_library(cryogen_test_main INTERFACE)
target_include_directories(cryogen_test_main INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

function(add_cryogen_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE cryogen::cryogen cryogen_test_main)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_cryogen_test(test_math)
add_cryogen_test(test_ecs)
add_cryogen_test(test_physics)
add_cryogen_test(test_net_replication)
