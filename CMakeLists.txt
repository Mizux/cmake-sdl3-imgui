cmake_minimum_required(VERSION 3.25)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

project(sdlimgui VERSION 0.1 LANGUAGES CXX)
message(STATUS "${PROJECT_NAME} version: ${PROJECT_VERSION}")
#message(STATUS "major: ${PROJECT_VERSION_MAJOR}")
#message(STATUS "minor: ${PROJECT_VERSION_MINOR}")
#message(STATUS "patch: ${PROJECT_VERSION_PATCH}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(isMultiConfig)
  if(NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "Release;Debug" CACHE STRING
    "Choose the type of builds, options are: Debug Release RelWithDebInfo MinSizeRel. (default: Release;Debug)"
    FORCE)
  endif()
  message(STATUS "Configuration types: ${CMAKE_CONFIGURATION_TYPES}")
else()
  if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING
    "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel. (default: Release)"
    FORCE)
  endif()
  message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
endif()

# Layout build dir like install dir
include(GNUInstallDirs)
if(UNIX)
  option(BUILD_SHARED_LIBS "Build shared libraries (.so or .dylib)." ON)
  set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
  # for multi-config build system (e.g. Xcode, Ninja Multi-Config)
  foreach(OutputConfig IN LISTS CMAKE_CONFIGURATION_TYPES)
    string(TOUPPER ${OutputConfig} OUTPUTCONFIG)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/${OutputConfig}/${CMAKE_INSTALL_LIBDIR})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/${OutputConfig}/${CMAKE_INSTALL_LIBDIR})
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/${OutputConfig}/${CMAKE_INSTALL_BINDIR})
  endforeach()
else()
  # Currently Only support static build for windows
  option(BUILD_SHARED_LIBS "Build shared libraries (.dll)." OFF)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
  # for multi-config builds (e.g. msvc)
  foreach(OutputConfig IN LISTS CMAKE_CONFIGURATION_TYPES)
    string(TOUPPER ${OutputConfig} OUTPUTCONFIG)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/${OutputConfig}/${CMAKE_INSTALL_BINDIR})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/${OutputConfig}/${CMAKE_INSTALL_BINDIR})
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/${OutputConfig}/${CMAKE_INSTALL_BINDIR})
  endforeach()
endif()

# Disable CTest targets
set_property(GLOBAL PROPERTY CTEST_TARGETS_ADDED 1)
include(CTest)

# SDL
if(${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
  if(NOT TARGET SDL3::SDL3)
    add_library(SDL3::SDL3 INTERFACE IMPORTED GLOBAL)
    target_compile_options(SDL3::SDL3 INTERFACE "-sUSE_SDL=3")
    target_link_options(SDL3::SDL3 INTERFACE "-sUSE_SDL=3")
  endif()
  if(NOT TARGET SDL3_image::SDL3_image)
    add_library(SDL3_image::SDL3_image INTERFACE IMPORTED GLOBAL)
  endif()
else()
  find_package(SDL3 REQUIRED)
  find_package(SDL3_image REQUIRED)
endif()

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

# IMGUI
message(CHECK_START "Fetching imgui")
list(APPEND CMAKE_MESSAGE_INDENT "  ")
FetchContent_Declare(
  imgui
  GIT_REPOSITORY "https://github.com/ocornut/imgui.git"
  GIT_TAG "v1.92.8"
  GIT_SHALLOW TRUE
  UPDATE_COMMAND git reset --hard
  PATCH_COMMAND git apply --ignore-whitespace
    "${CMAKE_CURRENT_LIST_DIR}/patches/imgui.patch"
  OVERRIDE_FIND_PACKAGE
  SYSTEM
)
#set(IMGUI_XXX OFF)
FetchContent_MakeAvailable(imgui)
list(POP_BACK CMAKE_MESSAGE_INDENT)
message(CHECK_PASS "fetched")


# Main App
file(GLOB_RECURSE _SRCS src/*.cpp src/*.hpp)
add_executable(${PROJECT_NAME} ${_SRCS})
target_include_directories(${PROJECT_NAME} PRIVATE src)
# note: macOS is APPLE and also UNIX !
if(APPLE)
  set_target_properties(${PROJECT_NAME} PROPERTIES
    INSTALL_RPATH "@loader_path/../${CMAKE_INSTALL_LIBDIR}")
elseif(UNIX AND NOT APPLE)
  set_target_properties(${PROJECT_NAME} PROPERTIES
    INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
endif()
target_link_libraries(${PROJECT_NAME} PRIVATE
  SDL3::SDL3
  SDL3_image::SDL3_image
  imgui::imgui
  imgui::imgui_impl_sdl3
  imgui::imgui_impl_opengl3
)
add_executable(${PROJECT_NAMESPACE}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})
