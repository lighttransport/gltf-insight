cmake_minimum_required(VERSION 3.5)

set(BUILD_TARGET "gltf-insight")

project(${BUILD_TARGET})

if(NOT IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glfw/include")
  message(FATAL_ERROR "The glfw submodule directory is missing! "
    "You probably did not clone submodules. It is possible to recover "
    "by running \"git submodule update --init\"")
endif()


find_package(OpenGL REQUIRED)

include_directories(${OPENGL_INCLUDE_DIR})

# local glad
include_directories(${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/deps")

set(SOURCES
    src/main.cc
    src/gltf-loader.cc
)

list(APPEND UI_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/deps/glad/src/glad.c)

# [ImGUi]
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui)
list(APPEND UI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui_widgets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui_draw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/examples/imgui_impl_glfw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/examples/imgui_impl_opengl2.cpp
    )

# Use nanogui's glfw3
set(GLFW_BUILD_EXAMPLES OFF CACHE INTERNAL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE INTERNAL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE INTERNAL "" FORCE)
set(GLFW_INSTALL OFF CACHE INTERNAL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/glfw)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/glfw/include)
list(APPEND EXT_LIBRARIES glfw)

add_executable(${BUILD_TARGET} ${SOURCES} ${UI_SOURCES})

target_link_libraries(
    ${BUILD_TARGET}
    ${OPENGL_LIBRARIES}
    ${EXT_LIBRARIES}
)

# # Increase warning level for clang.
# IF (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
#     target_compile_options(${BUILD_TARGET} PRIVATE -Weverything -Wno-system-headers -Werror -Wno-padded -Wno-c++98-compat-pedantic -Wno-documentation -Wno-documentation-unknown-command -Wno-reserved-id-macro)
# ENDIF ()



source_group("Source Files" FILES ${SOURCES})

# [VisualStudio]
if (WIN32)
  # Set `gltf-insight` as a startup project for VS IDE
  if (CMAKE_VERSION VERSION_GREATER 3.6.0)
	  set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT ${BUILD_TARGET})
  endif ()

  # For easier debugging in VS IDE(cmake 3.8.0 or later required)
  if (CMAKE_VERSION VERSION_GREATER 3.8.0)
    # Set working directory to $gltf-insight git repo root.
    set_target_properties(${BUILD_TARGET} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
  endif ()
endif ()

