cmake_minimum_required(VERSION 3.16)
project(lean_make_to_stock CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(USE_CBC "Build the CbcMipSolver backend (requires COIN-OR CBC dev libraries)" OFF)

include_directories(include)

set(CORE_SOURCES
    src/LeanMtsProblem.cpp
    src/GreedyMarginalCostSolver.cpp
    src/ExactDPSolver.cpp
)

add_library(leanmts_core ${CORE_SOURCES})
target_include_directories(leanmts_core PUBLIC include)

if(USE_CBC)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(CBC REQUIRED cbc)
  target_compile_definitions(leanmts_core PUBLIC LEANMTS_USE_CBC)
  target_include_directories(leanmts_core PUBLIC ${CBC_INCLUDE_DIRS})
  target_link_libraries(leanmts_core PUBLIC ${CBC_LIBRARIES})
endif()

add_executable(leanmts_demo src/main.cpp)
target_link_libraries(leanmts_demo PRIVATE leanmts_core)

add_executable(leanmts_tests
    tests/main.cpp
    tests/LeanMtsTests.cpp
)
target_include_directories(leanmts_tests PRIVATE tests)
target_link_libraries(leanmts_tests PRIVATE leanmts_core)

enable_testing()
add_test(NAME unit_tests COMMAND leanmts_tests)
