# Currently very minimal.

include(FindPackageHandleStandardArgs)

find_path(GLFW_INCLUDE_DIR GLFW/glfw3.h)
set(GLFW_INCLUDE_DIRS ${GLFW_INCLUDE_DIR})

find_library(GLFW_LIBRARY_RELEASE
    NAMES glfw3
    PATH_SUFFIXES lib/Release lib
    )
find_library(GLFW_LIBRARY_DEBUG
    NAMES glfw3d glfw3
    PATH_SUFFIXES lib/Debug lib
    )
set(GLFW_LIBRARIES
    optimized ${GLFW_LIBRARY_RELEASE}
    debug ${GLFW_LIBRARY_DEBUG}
    )

find_package_handle_standard_args(GLFW DEFAULT_MSG
    GLFW_LIBRARY_RELEASE
    GLFW_LIBRARY_DEBUG
    GLFW_INCLUDE_DIR
    )
