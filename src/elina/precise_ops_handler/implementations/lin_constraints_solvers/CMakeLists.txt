cmake_minimum_required(VERSION 3.10)
project(LinConstraintsSolvers)

# Set C++ standard (C++17 or newer)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set output directory for shared libraries
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

##
## Fetch the Prerequisites
##

## Gurobi
find_package(Gurobi REQUIRED)
if(GUROBI_FOUND)
    message(STATUS "GUROBI Include Dir: ${GUROBI_INCLUDE_DIR}")
    message(STATUS "GUROBI Libraries: ${GUROBI_CPP_LIBRARY} ${GUROBI_LIBRARY}")
    message(STATUS "GUROBI Library Dirs: ${GUROBI_LIBRARY_DIRS}")
else()
    message(FATAL_ERROR "GUROBI not found! Set GUROBI_PREFIX manually if needed.")
endif()

## OpenMP
find_package(OpenMP REQUIRED)

# Libtorch
find_package(Libtorch REQUIRED)
if(LIBTORCH_FOUND)
    message(STATUS "LIBTORCH Include Dirs: ${LIBTORCH_INCLUDE_DIRS}")
    message(STATUS "LIBTORCH Library Dirs: ${LIBTORCH_LIBRARY_DIRS}")
    message(STATUS "LIBTORCH Libraries: ${LIBTORCH_LIBS}")
else()
    message(FATAL_ERROR "LIBTORCH not found! Set LIBTORCH_PREFIX manually if needed.")
endif()

# Z3
find_package(Z3 REQUIRED)
if(Z3_FOUND)
    message(STATUS "Z3 Include Dirs: ${Z3_INCLUDE_DIRS}")
    message(STATUS "Z3 Library Dirs: ${Z3_LIBRARY_DIRS}")
    message(STATUS "Z3 Libraries: ${Z3_LIBS}")
else()
    message(FATAL_ERROR "Z3 not found! Set Z3_PREFIX manually if needed.")
endif()

# Gather all solver source files (each solver has a .cpp and .hpp)
file(GLOB SOLVER_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

# Iterate over each source file and create a shared library
foreach(SOLVER_SRC ${SOLVER_SOURCES})
    # Extract just the filename without extension
    get_filename_component(FILENAME ${SOLVER_SRC} NAME_WE)

    # Ensure filename matches expected pattern before extracting
    if(FILENAME MATCHES "^(.*)_lin_constraints_solver$")
        # Extract the part before "_lin_constraints_solver"
        string(REGEX REPLACE "^(.*)_lin_constraints_solver$" "\\1" SOLVER_NAME_PART ${FILENAME})

        add_library(${FILENAME} SHARED ${SOLVER_SRC})
        set_target_properties(${FILENAME} PROPERTIES PREFIX "")

        if(${SOLVER_NAME_PART} STREQUAL "gb")
            target_include_directories(${FILENAME} PRIVATE ${GUROBI_INCLUDE_DIR})

            target_link_directories(${FILENAME} PRIVATE ${GUROBI_LIBRARY_DIRS})
            set_target_properties(${FILENAME} PROPERTIES BUILD_RPATH ${GUROBI_LIBRARY_DIRS})
        
            target_link_libraries(${FILENAME} PRIVATE OpenMP::OpenMP_CXX)
            target_link_libraries(${FILENAME} PRIVATE ${GUROBI_CPP_LIBRARY})
            target_link_libraries(${FILENAME} PRIVATE ${GUROBI_LIBRARY})
        elseif(${SOLVER_NAME_PART} STREQUAL "dual")
            target_include_directories(${FILENAME} PRIVATE ${LIBTORCH_INCLUDE_DIRS})

            target_link_directories(${FILENAME} PRIVATE ${LIBTORCH_LIBRARY_DIRS})
            set_target_properties(${FILENAME} PROPERTIES BUILD_RPATH ${LIBTORCH_LIBRARY_DIRS})
        
            target_link_libraries(${FILENAME} PRIVATE ${LIBTORCH_LIBS})
        elseif(${SOLVER_NAME_PART} STREQUAL "symba")
            target_include_directories(${FILENAME} PRIVATE ${Z3_INCLUDE_DIRS})

            target_link_directories(${FILENAME} PRIVATE ${Z3_LIBRARY_DIRS})
            set_target_properties(${FILENAME} PROPERTIES BUILD_RPATH ${Z3_LIBRARY_DIRS})
        
            target_link_libraries(${FILENAME} PRIVATE OpenMP::OpenMP_CXX)
            target_link_libraries(${FILENAME} PRIVATE ${Z3_LIBS})
        endif()
    else()
        message(WARNING "Unexpected filename format: ${FILENAME}")
    endif()
endforeach()
