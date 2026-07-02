# Set the minimum required CMake version
cmake_minimum_required(VERSION 3.10)

# Define the project name
project(PreciseOpsHandler)

# Set C++ standard (C++17 or newer)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set where compiled binaries and libraries will be stored
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)  # Executables go here
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)  # Shared libraries go here

##
## Global includes
##
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/implementations/constraints_templates/include)
include_directories(${CMAKE_SOURCE_DIR}/../elina_auxiliary)

##
## Fetch the Prerequisites
##
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

## Boost

# Allow the user to specify BOOST_PREFIX manually
if(NOT DEFINED BOOST_PREFIX AND DEFINED ENV{BOOST_PREFIX})
    set(BOOST_PREFIX $ENV{BOOST_PREFIX})
endif()

# If BOOST_PREFIX is set, adjust search paths
if(BOOST_PREFIX)
    list(APPEND CMAKE_PREFIX_PATH ${BOOST_PREFIX})
endif()

find_package(Boost REQUIRED COMPONENTS json)
if(Boost_FOUND)
    message(STATUS "Boost Include Dir: ${Boost_INCLUDE_DIRS}")
    message(STATUS "Boost Libraries: ${Boost_LIBRARIES}")
    include_directories(${Boost_INCLUDE_DIRS})
else()
    message(FATAL_ERROR "Boost not found!")
endif()

# # Add subdirectories (each implementation has its own CMakeLists.txt)
add_subdirectory(implementations/lin_constraints_solvers)
add_subdirectory(implementations/quad_constraints_solvers)
add_subdirectory(implementations/constraints_templates)

# Compile common source files into a library
add_library(precise_ops SHARED
    src/quad_affine_block.cpp
    src/quad_affine_solver.cpp
    src/constraints_template.cpp
    src/lin_constraints_solver.cpp
    src/quad_constraints_solver.cpp
    src/utils.cpp
)

##
## Setup libs
##
target_link_libraries(precise_ops PUBLIC constraints_templates_impl)
target_link_libraries(precise_ops PUBLIC Boost::json)

##
## Install specific commands
##

# Read Makefile.config and extract ELINA_PREFIX
file(READ "${CMAKE_SOURCE_DIR}/../Makefile.config" MAKEFILE_CONFIG_CONTENTS)
string(REGEX MATCH "ELINA_PREFIX[ \t]*=[ \t]*([^ \n]+)" _match "${MAKEFILE_CONFIG_CONTENTS}")
set(ELINA_PREFIX "${CMAKE_MATCH_1}")

# If ELINA_PREFIX was not found, default to /usr/local
if(NOT ELINA_PREFIX)
    set(ELINA_PREFIX "/usr/local")
endif()

message(STATUS "Using ELINA_PREFIX: ${ELINA_PREFIX}")

# Define installation paths
# Override CMake's install prefix (ensures everything installs under ELINA_PREFIX)
set(CMAKE_INSTALL_PREFIX "${ELINA_PREFIX}" CACHE PATH "Installation directory" FORCE)
set(LIBDIR "${ELINA_PREFIX}/lib")
set(INCLDIR "${ELINA_PREFIX}/include")
message(STATUS "Using LIBDIR: ${LIBDIR}")

# Install build shared libs
install(DIRECTORY ${CMAKE_BINARY_DIR}/lib/ DESTINATION ${LIBDIR} FILES_MATCHING PATTERN "*.so" PATTERN "*.a" PATTERN "*.dylib")

# Install headers
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/ DESTINATION ${INCLDIR})