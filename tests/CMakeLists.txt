add_executable(mc_tests
    TestMain.cpp
    ScaleTests.cpp
    ChordTests.cpp
    ModelTests.cpp
    InputTests.cpp
    IoTests.cpp
)
target_link_libraries(mc_tests PRIVATE mc_core)
add_test(NAME mc_tests COMMAND mc_tests)
