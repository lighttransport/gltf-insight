cmake_minimum_required(VERSION 3.5)

set(BUILD_TARGET "gltf-insight")

project(${BUILD_TARGET})

if(NOT IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glfw/include")
  message(FATAL_ERROR "The glfw submodule directory is missing! "
    "You probably did not clone submodules. It is possible to recover "
    "by running \"git submodule update --init --recursive\" on top-level directory")
endif()

# cmake modules
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/sanitizers)
find_package(Sanitizers) # Address sanitizer (-DSANITIZE_ADDRESS=ON)

set (CMAKE_CXX_STANDARD 11)

find_package(OpenGL REQUIRED)
# OpenGL
include_directories(${OPENGL_INCLUDE_DIR})

# local glad
include_directories(${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/deps")
list(APPEND UI_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/deps/glad/src/glad.c)

# [glfw]
set(GLFW_BUILD_EXAMPLES OFF CACHE INTERNAL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE INTERNAL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE INTERNAL "" FORCE)
set(GLFW_INSTALL OFF CACHE INTERNAL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/glfw)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/glfw/include)
list(APPEND EXT_LIBRARIES glfw)

# [glm]
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/glm)

# [ImGUI], [ImGuizmo] and [ImGuiFileDialog]
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/ImGuizmo)
list(APPEND UI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui_widgets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui_draw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/examples/imgui_impl_glfw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/examples/imgui_impl_opengl2.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui_demo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ImGuizmo/ImSequencer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ImGuizmo/ImCurveEdit.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ImGuizmo/ImGradient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ImGuizmo/ImGuizmo.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/third_party/ImGuiFileDialog/file_dialog/ImGuiFileDialog.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/third_party/ImGuiFileDialog/file_dialog/ImGuiFileDialog.h
    )
include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}/third_party/ImGuiFileDialog/file_dialog/
	${CMAKE_CURRENT_SOURCE_DIR}/fonts/
)
# Required on non Unix platform to build ImGuiFileDialog
if(WIN32)
include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}/third_party/ImGuiFileDialog/dirent/include
)
endif()
#end of [ImGui] and related libraries inclusion

# [tinygltf]
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/tinygltf)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  file(GLOB SOURCES CONFIGURE_DEPENDS  src/*)
else()
  file(GLOB SOURCES src/*)
endif()

add_executable(${BUILD_TARGET} ${SOURCES} ${UI_SOURCES})
target_include_directories(${BUILD_TARGET} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/)
add_sanitizers(${BUILD_TARGET})

target_link_libraries(
    ${BUILD_TARGET}
    ${OPENGL_LIBRARIES}
    ${EXT_LIBRARIES}
)

# # Increase warning level for clang.
# IF (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
#     target_compile_options(${BUILD_TARGET} PRIVATE -Weverything -Wno-system-headers -Werror -Wno-padded -Wno-c++98-compat-pedantic -Wno-documentation -Wno-documentation-unknown-command -Wno-reserved-id-macro)
# ENDIF ()

# [VisualStudio]
if (WIN32)
  # Set `gltf-insight` as a startup project for VS IDE
  set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT ${BUILD_TARGET})

  # For easier debugging in VS IDE(cmake 3.8.0 or later required)
  # Set working directory to $gltf-insight git repo root.
  if (CMAKE_VERSION VERSION_GREATER 3.8.0)
	set_target_properties(${BUILD_TARGET} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
  endif()
endif ()

# ImGui Font Compressor uitility
add_executable(imgui_font_compressor_utility ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/misc/fonts/binary_to_compressed_c.cpp)
