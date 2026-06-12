add_executable(verosim_tests test_smoke.cpp)
target_link_libraries(verosim_tests PRIVATE verosim_core Catch2::Catch2WithMain)
include(Catch)
catch_discover_tests(verosim_tests)
