# Allow manually setting Z3_PREFIX or reading from environment
if(NOT DEFINED Z3_PREFIX AND DEFINED ENV{Z3_PREFIX})
    set(Z3_PREFIX $ENV{Z3_PREFIX})
endif()

# Default search paths (Check Z3_PREFIX first, then system paths)
set(Z3_SEARCH_PATHS
    "${Z3_PREFIX}"
    "/usr/local"
    "/usr"
    "/opt/homebrew"
)

# Headers to find
set(Z3_HEADERS
    "z3++.h"
)

# Libraries to find
set(Z3_LIBRARIES
    "z3"
)


# Find Headers
set(Z3_INCLUDE_DIRS "")
foreach(HEADER ${Z3_HEADERS})
    find_path(Z3_${HEADER}_DIR
        NAMES ${HEADER}
        PATHS ${Z3_SEARCH_PATHS}
        PATH_SUFFIXES include
        NO_DEFAULT_PATH
    )

    if(Z3_${HEADER}_DIR)
        list(APPEND Z3_INCLUDE_DIRS ${Z3_${HEADER}_DIR})
    endif()
endforeach()

# Find Libraries
set(Z3_LIBRARY_DIRS "")
set(Z3_LIBS "")
foreach(LIB ${Z3_LIBRARIES})
    find_library(Z3_${LIB}
        NAMES ${LIB}
        PATHS ${Z3_SEARCH_PATHS}
        PATH_SUFFIXES lib
        NO_DEFAULT_PATH
    )

    if(Z3_${LIB})
        get_filename_component(LIB_DIR ${Z3_${LIB}} DIRECTORY)
        list(APPEND Z3_LIBRARY_DIRS ${LIB_DIR})

        # Append library in order
        list(APPEND Z3_LIBS ${Z3_${LIB}})
    endif()
endforeach()

# Remove duplicates
list(REMOVE_DUPLICATES Z3_INCLUDE_DIRS)
list(REMOVE_DUPLICATES Z3_LIBRARY_DIRS)
list(REMOVE_DUPLICATES Z3_LIBS)

# Ensure All Dependencies Are Found
set(Z3_FOUND TRUE)
foreach(HEADER ${Z3_HEADERS})
    if(NOT Z3_${HEADER}_DIR)
        set(Z3_FOUND FALSE)
        message(ERROR "Missing Z3 Header: ${HEADER}")
    endif()
endforeach()
foreach(LIB ${Z3_LIBRARIES})
    if(NOT Z3_${LIB})
        set(Z3_FOUND FALSE)
        message(ERROR "Missing Z3 Library: ${LIB}")
    endif()
endforeach()

# Fail if missing
if(NOT Z3_FOUND)
    message(FATAL_ERROR "Z3 not found! Set Z3_PREFIX manually if needed.")
endif()

# Mark Variables as Advanced (To Keep `ccmake` Output Clean)
mark_as_advanced(${Z3_INCLUDE_DIRS} ${Z3_LIBRARY_DIRS} ${Z3_LIBS})
