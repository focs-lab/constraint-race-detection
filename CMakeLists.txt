cmake_minimum_required(VERSION 3.16)
project(CppProject)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Define project directories
set(SRC_DIR "${CMAKE_SOURCE_DIR}/src")
set(TEST_DIR "${CMAKE_SOURCE_DIR}/tests")

# Define dependencies
include_directories(/opt/homebrew/opt/z3/include)
link_directories(/opt/homebrew/opt/z3/lib)

# include_directories(/Users/rama/Desktop/FYP/z3/src/api/c++)
# include_directories(/Users/rama/Desktop/FYP/z3/src/api)
# link_directories(/Users/rama/Desktop/FYP/z3/build)

# include_directories(/opt/homebrew/opt/googletest/include)
# link_directories(/opt/homebrew/opt/googletest/lib)

# Define source files
file(GLOB PREDICTOR_SOURCES "${SRC_DIR}/*.cpp")
list(REMOVE_ITEM PREDICTOR_SOURCES "${SRC_DIR}/verifier.cpp")

file(GLOB VERIFIER_SOURCES 
    "${SRC_DIR}/verifier.cpp"
    ${SRC_DIR}/model_logger.cpp
    ${SRC_DIR}/trace.cpp
)

# Predictor executable
add_executable(predictor ${PREDICTOR_SOURCES})
target_link_libraries(predictor z3)

# Verifier executable
add_executable(verifier ${VERIFIER_SOURCES})
target_link_libraries(verifier z3)

# # Test executable
# file(GLOB TEST_SOURCES "${TEST_DIR}/*.cpp")
# add_executable(run_tests ${TEST_SOURCES})
# target_include_directories(run_tests PRIVATE ${SRC_DIR})
# target_link_libraries(run_tests gtest gtest_main pthread z3)

# # Custom targets
# add_custom_target(test
#     COMMAND ./run_tests
#     DEPENDS run_tests
#     WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
# )
