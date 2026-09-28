cmake_minimum_required(VERSION 3.16)
project(planar_stitcher_demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

find_package(OpenCV REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(EGL REQUIRED egl)
pkg_check_modules(GLES REQUIRED glesv2)
find_package(X11 REQUIRED)
find_package(Threads REQUIRED)

set(LIB_DIR ${CMAKE_SOURCE_DIR}/lib)
set(STITCHER_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)
set(MPP_ROOT "${CMAKE_SOURCE_DIR}/../../../multimedia/mpp/" CACHE PATH "MPP source directory")
get_filename_component(MPP_ROOT "${MPP_ROOT}" ABSOLUTE)
if(NOT EXISTS "${MPP_ROOT}/CMakeLists.txt")
  message(FATAL_ERROR "MPP source tree not found at ${MPP_ROOT}")
endif()
set(BUILD_TESTS OFF CACHE BOOL "Build MPP tests" FORCE)
add_subdirectory("${MPP_ROOT}" "${CMAKE_BINARY_DIR}/mpp" EXCLUDE_FROM_ALL)

# MPP is a subproject here, so CMAKE_SOURCE_DIR points at this project, not MPP.
# mpi/mux/CMakeLists.txt uses ${CMAKE_SOURCE_DIR}/mpi/demux to find codec/h264_utils.h
# and other demux helpers; that path resolves to a non-existent directory and
# breaks the build. Add the real path here to avoid patching the shared MPP checkout.
if(TARGET mpp_mux)
  target_include_directories(mpp_mux PRIVATE "${MPP_ROOT}/mpi/demux")
else()
  message(FATAL_ERROR "mpp_mux target missing: MPP layout changed")
endif()

add_library(planar_stitcher_core STATIC IMPORTED)
set_target_properties(planar_stitcher_core PROPERTIES
  IMPORTED_LOCATION ${LIB_DIR}/libplanar_stitcher_core.a)

add_executable(planar_stitcher stitcher_test/planar_stitcher_main.cpp)
if(TARGET vi_k3_cam_plugin)
  add_dependencies(planar_stitcher vi_k3_cam_plugin)
elseif(TARGET vi_k1_cam_plugin)
  add_dependencies(planar_stitcher vi_k1_cam_plugin)
endif()
target_include_directories(planar_stitcher PRIVATE
  ${STITCHER_INCLUDE_DIR}
  ${OpenCV_INCLUDE_DIRS}
  ${MPP_ROOT}/include
  ${MPP_ROOT}/include/sys
  ${MPP_ROOT}/include/vi
  ${MPP_ROOT}/include/vdec
  ${MPP_ROOT}/include/venc
  ${MPP_ROOT}/include/v2d
  ${EGL_INCLUDE_DIRS}
  ${GLES_INCLUDE_DIRS}
  ${X11_INCLUDE_DIR})
target_link_libraries(planar_stitcher PRIVATE
  planar_stitcher_core
  ${OpenCV_LIBS}
  mpp
  ${EGL_LIBRARIES}
  ${GLES_LIBRARIES}
  ${X11_LIBRARIES}
  Threads::Threads
  dl)

get_filename_component(_opencv_libdir "${OpenCV_DIR}/../.." ABSOLUTE)
set_target_properties(planar_stitcher PROPERTIES
  BUILD_RPATH "${LIB_DIR}:${_opencv_libdir}")
