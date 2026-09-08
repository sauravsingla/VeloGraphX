if(SKBUILD)
  find_package(pybind11 CONFIG REQUIRED)
else()
  find_package(pybind11 CONFIG QUIET)
endif()

if(pybind11_FOUND)
  pybind11_add_module(velographx_python bindings.cpp)
  target_include_directories(velographx_python PRIVATE ${PROJECT_SOURCE_DIR}/include)
  set_target_properties(velographx_python PROPERTIES OUTPUT_NAME velographx)

  if(SKBUILD)
    install(TARGETS velographx_python
      LIBRARY DESTINATION .
      RUNTIME DESTINATION .
      COMPONENT python
    )
  endif()
else()
  message(STATUS "pybind11 not found; Python extension skipped")
endif()
