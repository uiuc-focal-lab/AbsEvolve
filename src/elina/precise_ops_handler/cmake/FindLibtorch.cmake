# Allow manually setting LIBTORCH_PREFIX or reading from environment
if(NOT DEFINED LIBTORCH_PREFIX AND DEFINED ENV{LIBTORCH_PREFIX})
    set(LIBTORCH_PREFIX $ENV{LIBTORCH_PREFIX})
endif()

# Default search paths (Check LIBTORCH_PREFIX first, then system paths)
set(LIBTORCH_SEARCH_PATHS
    "${LIBTORCH_PREFIX}"
    "/usr/local"
    "/usr"
    "/opt/homebrew"
)

# Headers to find
set(LIBTORCH_HEADERS
    "torch/torch.h"
    "torch/csrc/autograd/autograd.h"
)

# Libraries to find
set(LIBTORCH_LIBRARIES
    "torch_cpu"
    "torch_cuda"
    "c10"
    "c10_cuda"
)

set(LIBTORCH_OPTIONAL_LIBRARIES
    "torch_cuda"
    "c10_cuda"
)

# Find Headers
set(LIBTORCH_INCLUDE_DIRS "")
foreach(HEADER ${LIBTORCH_HEADERS})
    find_path(LIBTORCH_${HEADER}_DIR
        NAMES ${HEADER}
        PATHS ${LIBTORCH_SEARCH_PATHS}
        PATH_SUFFIXES include include/torch/csrc/api/include
        NO_DEFAULT_PATH
    )

    if(LIBTORCH_${HEADER}_DIR)
        list(APPEND LIBTORCH_INCLUDE_DIRS ${LIBTORCH_${HEADER}_DIR})
    endif()
endforeach()

# Find Libraries
set(LIBTORCH_LIBRARY_DIRS "")
set(LIBTORCH_LIBS "")
foreach(LIB ${LIBTORCH_LIBRARIES})
    find_library(LIBTORCH_${LIB}
        NAMES ${LIB}
        PATHS ${LIBTORCH_SEARCH_PATHS}
        PATH_SUFFIXES lib
        NO_DEFAULT_PATH
    )

    if(LIBTORCH_${LIB})
        get_filename_component(LIB_DIR ${LIBTORCH_${LIB}} DIRECTORY)
        list(APPEND LIBTORCH_LIBRARY_DIRS ${LIB_DIR})

        # Append library in order
        list(APPEND LIBTORCH_LIBS ${LIBTORCH_${LIB}})
    endif()
endforeach()

# Remove duplicates
list(REMOVE_DUPLICATES LIBTORCH_INCLUDE_DIRS)
list(REMOVE_DUPLICATES LIBTORCH_LIBRARY_DIRS)
list(REMOVE_DUPLICATES LIBTORCH_LIBS)

# Ensure All Dependencies Are Found
set(LIBTORCH_FOUND TRUE)
foreach(HEADER ${LIBTORCH_HEADERS})
    if(NOT LIBTORCH_${HEADER}_DIR)
        set(LIBTORCH_FOUND FALSE)
        message(ERROR "Missing Torch Header: ${HEADER}")
    endif()
endforeach()
foreach(LIB ${LIBTORCH_LIBRARIES})
    if(NOT LIBTORCH_${LIB})
        list(FIND LIBTORCH_OPTIONAL_LIBRARIES "${LIB}" INDEX)

        if (INDEX GREATER -1)
            message(WARNING " Missing Torch Library: ${LIB} (but is optional)")
        else()
            set(LIBTORCH_FOUND FALSE)
            message(ERROR " Missing Torch Library: ${LIB}")
        endif()

    endif()
endforeach()

# Fail if missing
if(NOT LIBTORCH_FOUND)
    message(FATAL_ERROR "LibTorch not found! Set LIBTORCH_PREFIX manually if needed.")
endif()

# Mark Variables as Advanced (To Keep `ccmake` Output Clean)
mark_as_advanced(${LIBTORCH_INCLUDE_DIRS} ${LIBTORCH_LIBRARY_DIRS} ${LIBTORCH_LIBS})
