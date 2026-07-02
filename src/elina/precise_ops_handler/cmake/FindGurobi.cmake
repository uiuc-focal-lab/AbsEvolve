# Allow setting GUROBI_PREFIX manually or from environment
if(NOT DEFINED GUROBI_PREFIX AND DEFINED ENV{GUROBI_PREFIX})
    set(GUROBI_PREFIX $ENV{GUROBI_PREFIX})
endif()

# Default fallback if not defined
if(NOT DEFINED GUROBI_PREFIX)
    set(GUROBI_PREFIX "/opt/gurobi")
endif()

# Locate the Gurobi include directory
find_path(GUROBI_INCLUDE_DIR NAMES gurobi_c.h PATHS ${GUROBI_PREFIX}/include NO_DEFAULT_PATH)

find_library(GUROBI_CPP_LIBRARY NAMES gurobi_c++ PATHS ${GUROBI_PREFIX}/lib NO_DEFAULT_PATH)

# First try `find_library()` with standard names
find_library(GUROBI_LIBRARY NAMES gurobi PATHS ${GUROBI_PREFIX}/lib NO_DEFAULT_PATH)

# If `find_library` fails, manually search for Gurobi but exclude `gurobi_light`
if(NOT GUROBI_LIBRARY)
    message(WARNING "find_library() failed. Searching manually for Gurobi versions...")

    # Find valid Gurobi shared libraries but exclude "gurobi_light" and other unwanted variants
    execute_process(
        COMMAND /bin/sh -c "
            find -L ${GUROBI_PREFIX}/lib -maxdepth 1 -type f -name 'libgurobi*.so' ! -name '*light*'
        "
        OUTPUT_VARIABLE GUROBI_LIB_CANDIDATES
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )

    if(GUROBI_LIB_CANDIDATES)
        # Convert output to a list
        string(REPLACE "\n" ";" GUROBI_LIB_CANDIDATES "${GUROBI_LIB_CANDIDATES}")

        # Extract possible Gurobi names (libgurobi110.so → gurobi110)
        set(GUROBI_LIB_NAMES "")
        foreach(LIB_PATH ${GUROBI_LIB_CANDIDATES})
            get_filename_component(LIB_NAME ${LIB_PATH} NAME_WE)  # Extract "libgurobi110"
            string(REPLACE "lib" "" LIB_NAME ${LIB_NAME})         # Remove "lib" prefix
            list(APPEND GUROBI_LIB_NAMES ${LIB_NAME})
        endforeach()

        # Try `find_library()` again with dynamically found names
        find_library(GUROBI_LIBRARY NAMES ${GUROBI_LIB_NAMES} PATHS ${GUROBI_PREFIX}/lib NO_DEFAULT_PATH)
    endif()
endif()

# Extract the library directory path if found
if(GUROBI_LIBRARY)
    get_filename_component(GUROBI_LIBRARY_DIRS ${GUROBI_LIBRARY} DIRECTORY)
endif()

# Ensure both include and library are found, else fail
if(GUROBI_INCLUDE_DIR AND GUROBI_LIBRARY AND GUROBI_CPP_LIBRARY)
    set(GUROBI_FOUND TRUE CACHE INTERNAL "Gurobi has been found")
else()
    set(GUROBI_FOUND FALSE CACHE INTERNAL "Gurobi was not found")
    message(FATAL_ERROR "Gurobi not found! Set GUROBI_PREFIX manually if needed.")
endif()

# Mark paths as advanced
mark_as_advanced(GUROBI_INCLUDE_DIR GUROBI_LIBRARY GUROBI_CPP_LIBRARY)
