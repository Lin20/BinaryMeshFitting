set(name "BinaryMeshFitting")

file(GLOB sources LIST_DIRECTORIES false *.cpp)
file(GLOB sources_gui LIST_DIRECTORIES false GUI/*.cpp)
normalize_file_list(sources "${sources}")
normalize_file_list(sources_gui "${sources_gui}")
add_executable(${name} ${sources} ${sources_gui})
add_precompiled_header(${name} PCH.h FORCEINCLUDE
    SOURCE_CXX PCH.cpp
    SOURCES ${sources} # i.e. not the GUI sources 
    )
update_deps_file("${sources}" "${sources_gui}")

set_property(DIRECTORY ${PROJECT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${name})

find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(GLFW REQUIRED)
find_package(GLM REQUIRED)
find_package(Vc REQUIRED)
find_package(FastNoiseSIMD REQUIRED)

set_target_properties(${name} PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    )

target_include_directories(${name} PRIVATE
    ${GLEW_INCLUDE_DIRS}
    ${GLFW_INCLUDE_DIRS}
    ${GLM_INCLUDE_DIRS}
    ${Vc_INCLUDE_DIR}
    ${FastNoiseSIMD_INCLUDE_DIRS}
    )

target_link_libraries(${name} PRIVATE
    ${OPENGL_gl_LIBRARY}
    ${GLEW_LIBRARIES}
    ${GLFW_LIBRARIES}
    ${Vc_LIBRARIES}
    ${FastNoiseSIMD_LIBRARIES}
    )
