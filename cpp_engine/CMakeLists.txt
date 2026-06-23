cmake_minimum_required(VERSION 3.16)
project(prakshep_physics LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Optimization flags
if(MSVC)
  add_compile_options(/O2 /W4)
else()
  add_compile_options(-O3 -Wall -Wextra -march=native)
  if(MINGW OR CYGWIN)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++ -static")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libgcc -static-libstdc++ -static")
  endif()
endif()

# Core physics static library
add_library(prakshep_physics STATIC
  src/atmosphere.cpp
  src/gravity.cpp
  src/aerodynamics.cpp
  src/propulsion.cpp
  src/rocket_config.cpp
  src/integrator.cpp
  src/flight_computer.cpp
  src/simulation.cpp
)
target_include_directories(prakshep_physics PUBLIC include)

# Optional pybind11 module
option(BUILD_PYTHON_MODULE "Build Python bindings" ON)
if(BUILD_PYTHON_MODULE)
  find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
  find_package(pybind11 CONFIG QUIET)
  if(NOT pybind11_FOUND)
    # Try pip-installed pybind11
    execute_process(
      COMMAND ${Python3_EXECUTABLE} -m pybind11 --cmakedir
      OUTPUT_VARIABLE pybind11_DIR
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    find_package(pybind11 CONFIG REQUIRED)
  endif()
  pybind11_add_module(prakshep_core src/bindings.cpp)
  target_link_libraries(prakshep_core PRIVATE prakshep_physics)
  install(TARGETS prakshep_core DESTINATION .)
endif()

# Simple test executable
option(BUILD_TESTS "Build test executable" ON)
if(BUILD_TESTS)
  add_executable(prakshep_test tests/test_main.cpp)
  target_link_libraries(prakshep_test PRIVATE prakshep_physics)
endif()
