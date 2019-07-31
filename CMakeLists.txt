cmake_minimum_required(VERSION 3.5)

if(EMSCRIPTEN)
  message(STATUS "Emscripten platform detected. Should use the GLES 3.0 codepath with WebGL")
endif()

#TODO print similar messsages for other platforms

set(BUILD_TARGET "gltf-insight")

set(BUILD_VERSION_MAJOR 0)
set(BUILD_VERSION_MINOR 1)
set(BUILD_VERSION_PATCH 0)

project(${BUILD_TARGET})

option(GLTF_INSIGHT_USE_CCACHE "Compile with ccache(if available. Linux only)" OFF)
option(GLTF_INSIGHT_USE_NATIVEFILEDIALOG "Use NativeFileDialog instead of ImGuiFileDialog for file browser(requires GTK3 on Linux)" OFF)
option(GLTF_INSIGHT_USE_JSONRPC "Use JSON-RPC(over HTTP) to communicate with other process" OFF)

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

#Use the statically linked runtime on Windows
if(MSVC)
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
  set(CMAKE_C_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
  set(CMAKE_C_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
endif(MSVC)


find_package(OpenGL REQUIRED)
# OpenGL
include_directories(${OPENGL_INCLUDE_DIR})


# [ccache]
if (GLTF_INSIGHT_USE_CCACHE)
  if (MSVC)
    # No ccache support
  else()
    find_program(CCACHE_EXE ccache)
    if (CCACHE_EXE)

      message(STATUS "Use ccache : " ${CCACHE_EXE})
      # CMAKE_C_COMPILER_LAUNCHER = available from cmake 3.4+)
      if (CMAKE_C_COMPILER_LAUNCHER)
        set(CMAKE_C_COMPILER_LAUNCHER "${CMAKE_C_COMPILER_LAUNCHER}" "${CCACHE_EXE}")
      else()
        set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_EXE}")
      endif()

      if (CMAKE_CXX_COMPILER_LAUNCHER)
        set(CMAKE_CXX_COMPILER_LAUNCHER "${CMAKE_CXX_COMPILER_LAUNCHER}" "${CCACHE_EXE}")
      else()
        set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_EXE}")
      endif()

    endif (CCACHE_EXE)
  endif()
endif()

set(EXTRA_SOURCES)

# [fmtlib]
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/third_party/fmt/include")
list(APPEND EXTRA_SOURCES
	"${CMAKE_CURRENT_SOURCE_DIR}/third_party/fmt/src/format.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/third_party/fmt/src/posix.cc")

# [civetweb]
if (GLTF_INSIGHT_USE_JSONRPC)

  include_directories("${CMAKE_CURRENT_SOURCE_DIR}/deps/civetweb/include")
  list(APPEND EXTRA_SOURCES 
  	"${CMAKE_CURRENT_SOURCE_DIR}/deps/civetweb/src/civetweb.c"
  	"${CMAKE_CURRENT_SOURCE_DIR}/deps/civetweb/src/CivetServer.cpp"
  )
  
  add_definitions(-DGLTF_INSIGHT_WITH_JSONRPC)
  
  if (WIN32)
    # winsock2
    list(APPEND EXT_LIBRARIES ws2_32 mswsock)
  endif ()

endif(GLTF_INSIGHT_USE_JSONRPC)


# [glfw]
if(NOT EMSCRIPTEN) #TODO any GLES 3.0 system, we can include GLES3 directly
# local glad
include_directories(${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/deps")
list(APPEND UI_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/deps/glad/src/glad.c)
 set(GLFW_BUILD_EXAMPLES OFF CACHE INTERNAL "" FORCE)
 set(GLFW_BUILD_TESTS OFF CACHE INTERNAL "" FORCE)
 set(GLFW_BUILD_DOCS OFF CACHE INTERNAL "" FORCE)
 set(GLFW_INSTALL OFF CACHE INTERNAL "" FORCE)
 add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/glfw)
 include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/glfw/include)
 list(APPEND EXT_LIBRARIES glfw)
else()
 message(STATUS "skipping includion of GLFW subdir. Will ask emcc to USE_GLFW=3")
endif()


# [glm]
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/glm)

# [ImGUI], [ImGuizmo] and [ImGuiFileDialog]
# TODO(LTE): Disable ImGuiFileDialog when using NativeFileDialog.
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/ImGuizmo)
list(APPEND UI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui_widgets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui_draw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/examples/imgui_impl_glfw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/examples/imgui_impl_opengl3.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui_demo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ImGuizmo/ImSequencer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ImGuizmo/ImCurveEdit.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ImGuizmo/ImGradient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ImGuizmo/ImGuizmo.cpp)


include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}/fonts/
	${CMAKE_CURRENT_SOURCE_DIR}/icons/
	${CMAKE_CURRENT_SOURCE_DIR}/config/
)

include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}/third_party/ImGuiFileDialog/file_dialog/
	${CMAKE_CURRENT_SOURCE_DIR}/third_party/tinyobjloader
)

list(APPEND UI_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ImGuiFileDialog/file_dialog/ImGuiFileDialog.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ImGuiFileDialog/file_dialog/ImGuiFileDialog.h

)

if (GLTF_INSIGHT_USE_NATIVEFILEDIALOG)
  add_definitions(-DGLTF_INSIGHT_WITH_NATIVEFILEDIALOG)
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/nativefiledialog/src/include)
  if (WIN32)
    list(APPEND UI_SOURCES
         ${CMAKE_CURRENT_SOURCE_DIR}/third_party/nativefiledialog/src/nfd_common.c
         ${CMAKE_CURRENT_SOURCE_DIR}/third_party/nativefiledialog/src/nfd_win.cpp)
  elseif (APPLE)
    list(APPEND UI_SOURCES
         ${CMAKE_CURRENT_SOURCE_DIR}/third_party/nativefiledialog/src/nfd_common.c
         ${CMAKE_CURRENT_SOURCE_DIR}/third_party/nativefiledialog/src/nfd_cocoa.m)
  else() # Assume linux
    # https://gist.github.com/fracek/3323924
    # Use the package PkgConfig to detect GTK+ headers/library files
    FIND_PACKAGE(PkgConfig REQUIRED)
    PKG_CHECK_MODULES(GTK3 REQUIRED gtk+-3.0)

    # Setup CMake to use GTK+, tell the compiler where to look for headers
    # and to the linker where to look for libraries
    INCLUDE_DIRECTORIES(${GTK3_INCLUDE_DIRS})
    LINK_DIRECTORIES(${GTK3_LIBRARY_DIRS})

    # Add other flags to the compiler
    ADD_DEFINITIONS(${GTK3_CFLAGS_OTHER})

    list(APPEND EXT_LIBRARIES ${GTK3_LIBRARIES})

    list(APPEND UI_SOURCES
         ${CMAKE_CURRENT_SOURCE_DIR}/third_party/nativefiledialog/src/nfd_common.c
         ${CMAKE_CURRENT_SOURCE_DIR}/third_party/nativefiledialog/src/nfd_gtk.c)

  endif()

endif (GLTF_INSIGHT_USE_NATIVEFILEDIALOG)

set(CI_BUILD -1)
if(DEFINED ENV{TRAVIS_COMMIT})
 set(IS_CI true)
 message(STATUS "This is a travis build")
 set(CI_NAME "Travis-CI")
 set(GIT_COMMIT "$ENV{TRAVIS_COMMIT}")
 set(CI_BUILD "$ENV{TRAVIS_BUILD_NUMBER}")

elseif(DEFINED ENV{APPVEYOR_REPO_COMMIT})
 set(IS_CI true)
 message(STATUS "This is an appveyor build")
 set(CI_NAME "AppVeyor")
 set(GIT_COMMIT "$ENV{APPVEYOR_REPO_COMMIT}")
 set(CI_BUILD "$ENV{APPVEYOR_BUILD_NUMBER}")

else()
message(STATUS "This is not a CI build")
 set(IS_CI false)
 set (CI_NAME "ThisIsNotACIBuild")
 execute_process(
 COMMAND git log -1 --format=%H
 WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
 OUTPUT_VARIABLE GIT_COMMIT
 OUTPUT_STRIP_TRAILING_WHITESPACE)

endif()
 execute_process(
 COMMAND git log -1 --format=%h
 WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
 OUTPUT_VARIABLE GIT_COMMIT_SHORT
 OUTPUT_STRIP_TRAILING_WHITESPACE)

configure_file(
 ${CMAKE_CURRENT_SOURCE_DIR}/config/cmake_config.hh.template
 ${CMAKE_CURRENT_SOURCE_DIR}/config/cmake_config.hh)

# Required on non Unix platform to build ImGuiFileDialog
if(WIN32)
include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}/third_party/ImGuiFileDialog/dirent/include
)
endif()
#end of [ImGui] and related libraries inclusion

# [tinygltf]
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/tinygltf)

# [cpp-optparse]
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/deps/cpp-optparse/)
list(APPEND EXTRA_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/deps/cpp-optparse/OptionParser.cpp)

#Discover the source code
if (CMAKE_VERSION VERSION_GREATER 3.12)
  file(GLOB SOURCES CONFIGURE_DEPENDS  src/*)
  file(GLOB SHADER CONFIGURE_DEPENDS shaders/*.vert shaders/*.frag)
else()
  file(GLOB SOURCES src/*)
  file(GLOB SHADER shaders/*.vert shaders/*.frag)
endif()

# Increase warning level for clang.
# Only apply source files of `gltf-insght`
# https://stackoverflow.com/questions/13638408/override-compile-flags-for-single-files
# TODO(LTE): GLOB is not recommended so do not use GLOB to list up source files at `SOURCES`
IF (CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT EMSCRIPTEN)
  # Assume C++ sources
  set_source_files_properties(${SOURCES} PROPERTIES COMPILE_FLAGS "-Weverything -Wno-system-headers -Werror -Wno-padded -Wno-c++98-compat-pedantic -Wno-documentation -Wno-documentation-unknown-command -Wno-reserved-id-macro -Wno-global-constructors -Wno-double-promotion -Wno-exit-time-destructors -Wno-float-equal")
ENDIF ()

#find xxd's executable
find_program(SHADER_PACKER xxd /usr/bin /usr/local/bin /bin ${CMAKE_CURRENT_SOURCE_DIR}/bin)

#Iterate over every shader file
foreach(INPUT_FILE ${SHADER})
	#From full path, get only the filename of the shader
	get_filename_component(INPUT_FILENAME ${INPUT_FILE} NAME)
	#Compute an output path that lands in build/shaders/shadername.{frag, vert}_inc.hh
	set(OUTPUT_FILE ${CMAKE_CURRENT_SOURCE_DIR}/config/shaders/${INPUT_FILENAME}_inc.hh)
	#A a custom build command that will generate the output file as a includable header
	add_custom_command(
		OUTPUT ${OUTPUT_FILE}
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/shaders/
		COMMAND  ${SHADER_PACKER}  -i  ${INPUT_FILENAME} ${OUTPUT_FILE}
		COMMENT  "packing ${INPUT_FILE} shader into ${OUTPUT_FILE}"
		DEPENDS ${INPUT_FILE}
	)

	list(APPEND SHADER_GEN ${OUTPUT_FILE})
endforeach()
# Shader source code is #included from shaders directory
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/config/shaders)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/shaders/)

if(CMAKE_VERSION VERSION_GREATER 3.8)
	source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES})
endif()

if(WIN32)
  set(ICO_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/icons/icons.rc)
endif()

add_executable(${BUILD_TARGET} ${SOURCES} ${EXTRA_SOURCES} ${UI_SOURCES} ${ICO_SOURCES} ${SHADER} ${SHADER_GEN}
	${CMAKE_CURRENT_SOURCE_DIR}/config/cmake_config.hh
	${CMAKE_CURRENT_SOURCE_DIR}/third_party/tinyobjloader/tiny_obj_loader.h
)

target_include_directories(
${BUILD_TARGET} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/
)

if(NOT EMSCRIPTEN)
add_definitions( -DIMGUI_IMPL_OPENGL_LOADER_GLAD)
target_include_directories(${BUILD_TARGET} PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/deps/glad/include
  )
endif()

add_sanitizers(${BUILD_TARGET})


if(APPLE)
  list(APPEND EXT_LIBRARIES "-framework Cocoa")
  list(APPEND EXT_LIBRARIES "-framework CoreFoundation" )
  list(APPEND EXT_LIBRARIES "-framework ApplicationServices")
endif()
if(EMSCRIPTEN)
  message(STATUS "configuring ${BUILD_TARGET} for emscripten GLFW3 port")
  #add_definitions("-DGLFW_INCLUDE_ES3")
  set_target_properties(${BUILD_TARGET} PROPERTIES SUFFIX ".html")
  target_compile_definitions(${BUILD_TARGET} PUBLIC -DEMSCRIPTEN=1)
  file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/gltf-samples DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/)
  set_target_properties(${BUILD_TARGET} PROPERTIES LINK_FLAGS "--emrun --no-heap-copy -s ALLOW_MEMORY_GROWTH=1 -s USE_GLFW=3 -s USE_WEBGL2=1 -s FULL_ES3=1 -s ERROR_ON_MISSING_LIBRARIES=0 -s ERROR_ON_UNDEFINED_SYMBOLS=0 --preload-file gltf-samples")
endif()

target_link_libraries(
    ${BUILD_TARGET}
    ${OPENGL_LIBRARIES}
    ${EXT_LIBRARIES}
    )

# Install the built executable into (prefix)/bin
install(TARGETS ${BUILD_TARGET} DESTINATION bin)

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")

  install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/icons/gltf-insight.desktop DESTINATION share/applications)
  install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/icons/gltf-insight.svg DESTINATION share/icons)
endif()


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
