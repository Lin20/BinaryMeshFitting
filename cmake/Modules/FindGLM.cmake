# Currently very minimal.

include(FindPackageHandleStandardArgs)

find_path(GLM_INCLUDE_DIR glm/glm.hpp)
set(GLM_INCLUDE_DIRS ${GLM_INCLUDE_DIR})

find_package_handle_standard_args(GLM DEFAULT_MSG
    GLM_INCLUDE_DIRS
    )
