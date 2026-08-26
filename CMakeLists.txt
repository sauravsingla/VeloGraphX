cmake_minimum_required(VERSION 3.20)
project(VeloGraphX VERSION 0.2.0 LANGUAGES CXX)

option(VELOGRAPHX_BUILD_TESTS "Build tests" ON)
option(VELOGRAPHX_BUILD_BENCHMARKS "Build benchmarks" ON)
option(VELOGRAPHX_BUILD_PYTHON "Build Python bindings" OFF)
option(VELOGRAPHX_ENABLE_SANITIZERS "Enable ASan/UBSan" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(velographx INTERFACE)
target_include_directories(velographx INTERFACE ${PROJECT_SOURCE_DIR}/include)
target_compile_features(velographx INTERFACE cxx_std_20)

if(VELOGRAPHX_ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

add_executable(velographx_example examples/basic.cpp)
target_link_libraries(velographx_example PRIVATE velographx)

add_executable(velographx_dynamic_example examples/dynamic_transactions.cpp)
target_link_libraries(velographx_dynamic_example PRIVATE velographx)

if(VELOGRAPHX_BUILD_TESTS)
  enable_testing()
  foreach(test_name IN ITEMS core dynamic incremental io)
    add_executable(velographx_test_${test_name} tests/test_${test_name}.cpp)
    target_link_libraries(velographx_test_${test_name} PRIVATE velographx)
    add_test(NAME ${test_name} COMMAND velographx_test_${test_name})
  endforeach()
endif()

if(VELOGRAPHX_BUILD_BENCHMARKS)
  add_executable(velographx_benchmark benchmarks/benchmark.cpp)
  target_link_libraries(velographx_benchmark PRIVATE velographx)
  add_executable(velographx_dynamic_benchmark benchmarks/dynamic_benchmark.cpp)
  target_link_libraries(velographx_dynamic_benchmark PRIVATE velographx)
endif()

if(VELOGRAPHX_BUILD_PYTHON)
  add_subdirectory(python)
endif()
