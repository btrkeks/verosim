add_executable(verosim_tests
    test_smoke.cpp
    test_cli_args.cpp
    test_check.cpp
    test_fraction.cpp
    test_notation_rules.cpp
    test_extract.cpp
    test_hand10.cpp
    test_engine_units.cpp
    test_engine_identity.cpp
    test_mutation_gate.cpp
    test_visual.cpp)
target_link_libraries(verosim_tests PRIVATE verosim_core Catch2::Catch2WithMain)
target_compile_definitions(verosim_tests PRIVATE
    VEROSIM_TEST_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
    VEROSIM_HAND10_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../corpora/hand10"
    VEROSIM_MUTATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../corpora/mutations")
include(Catch)
catch_discover_tests(verosim_tests)
