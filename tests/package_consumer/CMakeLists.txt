cmake_minimum_required(VERSION 3.20)
project(VeloGraphXPackageConsumer LANGUAGES CXX)

find_package(VeloGraphX CONFIG REQUIRED)

add_executable(velographx_package_consumer main.cpp)
target_link_libraries(velographx_package_consumer PRIVATE VeloGraphX::velographx)
