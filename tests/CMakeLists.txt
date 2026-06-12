add_executable(verosim_tests test_smoke.cpp test_check.cpp)
target_link_libraries(verosim_tests PRIVATE verosim_core Catch2::Catch2WithMain)
target_compile_definitions(verosim_tests PRIVATE
    VEROSIM_TEST_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
include(Catch)
catch_discover_tests(verosim_tests)
