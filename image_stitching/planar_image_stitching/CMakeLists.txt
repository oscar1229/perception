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
set(MPP_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include/mpp)

add_library(planar_stitcher_core STATIC IMPORTED)
set_target_properties(planar_stitcher_core PROPERTIES
  IMPORTED_LOCATION ${LIB_DIR}/libplanar_stitcher_core.a)

if(NOT EXISTS ${LIB_DIR}/libmpp.so OR
   NOT EXISTS ${MPP_INCLUDE_DIR}/vdec/vdec_api.h)
  message(FATAL_ERROR "Installed MPP library or headers are missing")
endif()

add_executable(planar_stitcher stitcher_test/planar_stitcher_main.cpp)
target_include_directories(planar_stitcher PRIVATE
  ${STITCHER_INCLUDE_DIR}
  ${OpenCV_INCLUDE_DIRS}
  ${MPP_INCLUDE_DIR}
  ${MPP_INCLUDE_DIR}/sys
  ${MPP_INCLUDE_DIR}/vi
  ${MPP_INCLUDE_DIR}/vdec
  ${MPP_INCLUDE_DIR}/venc
  ${MPP_INCLUDE_DIR}/v2d
  ${EGL_INCLUDE_DIRS}
  ${GLES_INCLUDE_DIRS}
  ${X11_INCLUDE_DIR})
target_link_libraries(planar_stitcher PRIVATE
  planar_stitcher_core
  ${OpenCV_LIBS}
  ${LIB_DIR}/libmpp.so
  ${EGL_LIBRARIES}
  ${GLES_LIBRARIES}
  ${X11_LIBRARIES}
  Threads::Threads
  dl)

get_filename_component(_opencv_libdir "${OpenCV_DIR}/../.." ABSOLUTE)
set_target_properties(planar_stitcher PROPERTIES
  BUILD_RPATH "${LIB_DIR}:${_opencv_libdir}")
