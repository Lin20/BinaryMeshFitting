# Currently very minimal.

find_path(FastNoiseSIMD_INCLUDE_DIR FastNoiseSIMD.h)
set(FastNoiseSIMD_INCLUDE_DIRS ${FastNoiseSIMD_INCLUDE_DIR})

find_library(FastNoiseSIMD_LIBRARY_RELEASE
    NAMES FastNoiseSIMD
    PATH_SUFFIXES lib/Release lib
    )
find_library(FastNoiseSIMD_LIBRARY_DEBUG
    NAMES FastNoiseSIMDd FastNoiseSIMD
    PATH_SUFFIXES lib/Debug lib
    )
set(FastNoiseSIMD_LIBRARIES
    optimized ${FastNoiseSIMD_LIBRARY_RELEASE}
    debug ${FastNoiseSIMD_LIBRARY_DEBUG}
    )

find_package_handle_standard_args(FastNoiseSIMD DEFAULT_MSG
    FastNoiseSIMD_LIBRARY_RELEASE
    FastNoiseSIMD_LIBRARY_DEBUG
    FastNoiseSIMD_INCLUDE_DIR
    )
