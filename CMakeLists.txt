cmake_minimum_required(VERSION 3.20)
project(VeloGraphX VERSION 0.1.0 LANGUAGES CXX)

option(VELOGRAPHX_BUILD_TESTS "Build tests" ON)
option(VELOGRAPHX_BUILD_BENCHMARKS "Build benchmarks" ON)
option(VELOGRAPHX_BUILD_EXAMPLES "Build examples" ON)

add_library(velographx
  src/csr_graph.cpp
  src/algorithms.cpp
  src/io.cpp
)
add_library(VeloGraphX::velographx ALIAS velographx)

target_compile_features(velographx PUBLIC cxx_std_20)
target_include_directories(velographx PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
if(MSVC)
  target_compile_options(velographx PRIVATE /W4)
else()
  target_compile_options(velographx PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(VELOGRAPHX_BUILD_EXAMPLES)
  add_executable(vgx_example examples/basic.cpp)
  target_link_libraries(vgx_example PRIVATE VeloGraphX::velographx)
endif()

if(VELOGRAPHX_BUILD_BENCHMARKS)
  add_executable(vgx_benchmark benchmarks/static_benchmark.cpp)
  target_link_libraries(vgx_benchmark PRIVATE VeloGraphX::velographx)
endif()

if(VELOGRAPHX_BUILD_TESTS)
  enable_testing()
  add_executable(vgx_tests tests/test_static.cpp)
  target_link_libraries(vgx_tests PRIVATE VeloGraphX::velographx)
  add_test(NAME velographx_static COMMAND vgx_tests)
endif()
