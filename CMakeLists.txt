cmake_minimum_required(VERSION 3.20)
project(VeloGraphX VERSION 0.8.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(VELOGRAPHX_BUILD_TESTS "Build tests" ON)
option(VELOGRAPHX_BUILD_BENCHMARKS "Build benchmarks" ON)
option(VELOGRAPHX_BUILD_PYTHON "Build Python bindings" OFF)
option(VELOGRAPHX_ENABLE_SANITIZERS "Enable ASan/UBSan" OFF)
option(VELOGRAPHX_ENABLE_IO_URING "Enable optional Linux liburing prefetch path" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(velographx STATIC
  src/csr_graph.cpp
  src/algorithms.cpp
  src/io.cpp
)
add_library(VeloGraphX::velographx ALIAS velographx)
target_include_directories(velographx PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_features(velographx PUBLIC cxx_std_20)

if(VELOGRAPHX_ENABLE_IO_URING)
  if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "VELOGRAPHX_ENABLE_IO_URING is supported only on Linux")
  endif()
  find_path(VELOGRAPHX_LIBURING_INCLUDE_DIR liburing.h)
  find_library(VELOGRAPHX_LIBURING_LIBRARY uring)
  if(NOT VELOGRAPHX_LIBURING_INCLUDE_DIR OR NOT VELOGRAPHX_LIBURING_LIBRARY)
    message(FATAL_ERROR "VELOGRAPHX_ENABLE_IO_URING=ON requires liburing headers and library")
  endif()
  target_include_directories(velographx PUBLIC ${VELOGRAPHX_LIBURING_INCLUDE_DIR})
  target_link_libraries(velographx PUBLIC ${VELOGRAPHX_LIBURING_LIBRARY})
  target_compile_definitions(velographx PUBLIC VELOGRAPHX_ENABLE_IO_URING=1)
endif()

if(VELOGRAPHX_ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(velographx PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(velographx PUBLIC -fsanitize=address,undefined)
endif()

set(VELOGRAPHX_CMAKE_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/VeloGraphX")

install(TARGETS velographx
  EXPORT VeloGraphXTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

configure_package_config_file(
  cmake/VeloGraphXConfig.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/VeloGraphXConfig.cmake"
  INSTALL_DESTINATION "${VELOGRAPHX_CMAKE_INSTALL_DIR}"
)
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/VeloGraphXConfigVersion.cmake"
  VERSION "${PROJECT_VERSION}"
  COMPATIBILITY SameMajorVersion
)
install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/VeloGraphXConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/VeloGraphXConfigVersion.cmake"
  DESTINATION "${VELOGRAPHX_CMAKE_INSTALL_DIR}"
)
install(EXPORT VeloGraphXTargets
  FILE VeloGraphXTargets.cmake
  NAMESPACE VeloGraphX::
  DESTINATION "${VELOGRAPHX_CMAKE_INSTALL_DIR}"
)

add_executable(velographx_example examples/basic.cpp)
target_link_libraries(velographx_example PRIVATE velographx)
add_executable(velographx_dynamic_example examples/dynamic_transactions.cpp)
target_link_libraries(velographx_dynamic_example PRIVATE velographx)

if(VELOGRAPHX_BUILD_TESTS)
  enable_testing()
  set(VELOGRAPHX_TESTS static dynamic consolidation_controller incremental incremental_bfs_deletion incremental_components_deletion kcore_localized pagerank_localized storage_independence graph_access_adl io io_hardening kernels weighted_sssp numa numa_policy numa_partitioner numa_scheduler work_stealing work_stealing_stress degree_frontier_scheduler execution_plan compression compression_policy temporal randomized_dynamic partition_cache partition_file async_partition_loader)
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

  configure_file(
    tests/package_consumer/run_consumer_test.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/run_package_consumer_test.cmake"
    @ONLY
  )
  add_test(
    NAME package_consumer
    COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_BINARY_DIR}/run_package_consumer_test.cmake"
  )
  set_tests_properties(package_consumer PROPERTIES TIMEOUT 120)
endif()

if(VELOGRAPHX_BUILD_BENCHMARKS)
  find_package(Threads REQUIRED)
  set(static_benchmark_source "${PROJECT_SOURCE_DIR}/benchmarks/static_benchmark.cpp")
  set(dynamic_benchmark_source "${PROJECT_SOURCE_DIR}/benchmarks/dynamic_benchmark.cpp")
  set(intersection_benchmark_source "${PROJECT_SOURCE_DIR}/benchmarks/intersection_benchmark.cpp")
  set(update_fraction_benchmark_source "${PROJECT_SOURCE_DIR}/benchmarks/update_fraction_campaign.cpp")
  set(compression_benchmark_source "${PROJECT_SOURCE_DIR}/benchmarks/compression_benchmark.cpp")
  set(public_dataset_benchmark_source "${PROJECT_SOURCE_DIR}/benchmarks/public_dataset_benchmark.cpp")
  set(public_update_fraction_benchmark_source "${PROJECT_SOURCE_DIR}/benchmarks/public_update_fraction_campaign.cpp")
  set(large_scale_triangle_benchmark_source "${PROJECT_SOURCE_DIR}/benchmarks/large_scale_triangle_campaign.cpp")
  set(thread_scaling_benchmark_source "${PROJECT_SOURCE_DIR}/benchmarks/thread_scaling_bfs.cpp")
  set(storage_ab_benchmark_source "${PROJECT_SOURCE_DIR}/benchmarks/storage_ab_benchmark.cpp")
  set(backend_bfs_benchmark_source "${PROJECT_SOURCE_DIR}/benchmarks/backend_bfs_benchmark.cpp")
  set(row_patch_accumulation_source "${PROJECT_SOURCE_DIR}/benchmarks/row_patch_accumulation.cpp")
  set(steady_state_storage_source "${PROJECT_SOURCE_DIR}/benchmarks/steady_state_storage.cpp")
  set(external_risgraph_bfs_source "${PROJECT_SOURCE_DIR}/benchmarks/external_risgraph_bfs.cpp")
  set(adaptive_policy_bfs_source "${PROJECT_SOURCE_DIR}/benchmarks/adaptive_policy_bfs.cpp")
  set(native_baseline_benchmark_source "${PROJECT_SOURCE_DIR}/benchmarks/native_baseline_benchmark.cpp")
  set(graphbolt_contract_bfs_source "${PROJECT_SOURCE_DIR}/benchmarks/graphbolt_contract_bfs.cpp")
  foreach(benchmark_source IN ITEMS "${static_benchmark_source}" "${dynamic_benchmark_source}" "${intersection_benchmark_source}" "${update_fraction_benchmark_source}" "${compression_benchmark_source}" "${public_dataset_benchmark_source}" "${public_update_fraction_benchmark_source}" "${large_scale_triangle_benchmark_source}" "${thread_scaling_benchmark_source}" "${storage_ab_benchmark_source}" "${backend_bfs_benchmark_source}" "${row_patch_accumulation_source}" "${steady_state_storage_source}" "${external_risgraph_bfs_source}" "${adaptive_policy_bfs_source}" "${native_baseline_benchmark_source}" "${graphbolt_contract_bfs_source}")
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
  add_executable(velographx_compression_benchmark ${compression_benchmark_source})
  target_link_libraries(velographx_compression_benchmark PRIVATE velographx)
  add_executable(velographx_public_dataset_benchmark ${public_dataset_benchmark_source})
  target_link_libraries(velographx_public_dataset_benchmark PRIVATE velographx)
  add_executable(velographx_public_update_fraction_benchmark ${public_update_fraction_benchmark_source})
  target_link_libraries(velographx_public_update_fraction_benchmark PRIVATE velographx)
  add_executable(velographx_large_scale_triangle_benchmark ${large_scale_triangle_benchmark_source})
  target_link_libraries(velographx_large_scale_triangle_benchmark PRIVATE velographx)
  add_executable(velographx_thread_scaling_bfs ${thread_scaling_benchmark_source})
  target_link_libraries(velographx_thread_scaling_bfs PRIVATE velographx Threads::Threads)
  add_executable(velographx_storage_ab_benchmark ${storage_ab_benchmark_source})
  target_link_libraries(velographx_storage_ab_benchmark PRIVATE velographx)
  add_executable(velographx_backend_bfs_benchmark ${backend_bfs_benchmark_source})
  target_link_libraries(velographx_backend_bfs_benchmark PRIVATE velographx)
  add_executable(velographx_row_patch_accumulation ${row_patch_accumulation_source})
  target_link_libraries(velographx_row_patch_accumulation PRIVATE velographx)
  add_executable(velographx_steady_state_storage ${steady_state_storage_source})
  target_link_libraries(velographx_steady_state_storage PRIVATE velographx)
  add_executable(velographx_external_risgraph_bfs ${external_risgraph_bfs_source})
  target_link_libraries(velographx_external_risgraph_bfs PRIVATE velographx)
  add_executable(velographx_adaptive_policy_bfs ${adaptive_policy_bfs_source})
  target_link_libraries(velographx_adaptive_policy_bfs PRIVATE velographx)
  add_executable(velographx_native_baseline_benchmark ${native_baseline_benchmark_source})
  target_link_libraries(velographx_native_baseline_benchmark PRIVATE velographx)
  add_executable(velographx_graphbolt_contract_bfs ${graphbolt_contract_bfs_source})
  target_link_libraries(velographx_graphbolt_contract_bfs PRIVATE velographx)
endif()

if(VELOGRAPHX_BUILD_PYTHON)
  add_subdirectory(python)
endif()
