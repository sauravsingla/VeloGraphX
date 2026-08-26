cmake_minimum_required(VERSION 3.20)
project(VeloGraphX VERSION 0.4.8 LANGUAGES CXX)

option(VELOGRAPHX_BUILD_TESTS "Build tests" ON)
option(VELOGRAPHX_BUILD_BENCHMARKS "Build benchmarks" ON)
option(VELOGRAPHX_BUILD_PYTHON "Build Python bindings" OFF)
option(VELOGRAPHX_ENABLE_SANITIZERS "Enable ASan/UBSan" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(velographx STATIC
  src/csr_graph.cpp
  src/algorithms.cpp
  src/io.cpp
)
target_include_directories(velographx PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_compile_features(velographx PUBLIC cxx_std_20)

if(VELOGRAPHX_ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(velographx PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(velographx PUBLIC -fsanitize=address,undefined)
endif()

add_executable(velographx_example examples/basic.cpp)
target_link_libraries(velographx_example PRIVATE velographx)

add_executable(velographx_dynamic_example examples/dynamic_transactions.cpp)
target_link_libraries(velographx_dynamic_example PRIVATE velographx)

if(VELOGRAPHX_BUILD_TESTS)
  enable_testing()
  set(VELOGRAPHX_TESTS static dynamic incremental io io_hardening kernels weighted_sssp numa numa_policy work_stealing work_stealing_stress degree_frontier_scheduler execution_plan compression temporal randomized_dynamic partition_cache partition_file async_partition_loader)
  foreach(test_name IN LISTS VELOGRAPHX_TESTS)
    set(test_source "${PROJECT_SOURCE_DIR}/tests/test_${test_name}.cpp")
    if(NOT EXISTS "${test_source}")
      message(FATAL_ERROR "Configured test source does not exist: ${test_source}")
    endif()
    add_executable(velographx_test_${test_name} ${test_source})
    target_link_libraries(velographx_test_${test_name} PRIVATE velographx)
    add_test(NAME ${test_name} COMMAND velographx_test_${test_name})
    set_tests_properties(${test_name} PROPERTIES TIMEOUT 60)
  endforeach()
endif()

if(VELOGRAPHX_BUILD_BENCHMARKS)
  set(static_benchmark_source "${PROJECT_SOURCE_DIR}/benchmarks/static_benchmark.cpp")
  set(dynamic_benchmark_source "${PROJECT_SOURCE_DIR}/benchmarks/dynamic_benchmark.cpp")
  set(intersection_benchmark_source "${PROJECT_SOURCE_DIR}/benchmarks/intersection_benchmark.cpp")
  set(update_fraction_benchmark_source "${PROJECT_SOURCE_DIR}/benchmarks/update_fraction_campaign.cpp")
  foreach(benchmark_source IN ITEMS "${static_benchmark_source}" "${dynamic_benchmark_source}" "${intersection_benchmark_source}" "${update_fraction_benchmark_source}")
    if(NOT EXISTS "${benchmark_source}")
      message(FATAL_ERROR "Configured benchmark source does not exist: ${benchmark_source}")
    endif()
  endforeach()
  add_executable(velographx_benchmark ${static_benchmark_source})
  target_link_libraries(velographx_benchmark PRIVATE velographx)
  add_executable(velographx_dynamic_benchmark ${dynamic_benchmark_source})
  target_link_libraries(velographx_dynamic_benchmark PRIVATE velographx)
  add_executable(velographx_intersection_benchmark ${intersection_benchmark_source})
  target_link_libraries(velographx_intersection_benchmark PRIVATE velographx)
  add_executable(velographx_update_fraction_benchmark ${update_fraction_benchmark_source})
  target_link_libraries(velographx_update_fraction_benchmark PRIVATE velographx)
endif()

if(VELOGRAPHX_BUILD_PYTHON)
  add_subdirectory(python)
endif()
