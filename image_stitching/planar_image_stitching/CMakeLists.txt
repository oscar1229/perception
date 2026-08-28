cmake_minimum_required(VERSION 3.16)
project(planar_stitcher_demo)

# 本工程仅依赖预编译库 (lib/*.a/*.so) 与安装头文件 (include/)
# 不包含 planar_stitcher 源码
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

# 导入预编译静态库
add_library(planar_stitcher_core STATIC IMPORTED)
set_target_properties(planar_stitcher_core PROPERTIES
    IMPORTED_LOCATION ${LIB_DIR}/libplanar_stitcher_core.a)

# MPP 共享库
set(STITCHER_MPP_LIB "")
if(EXISTS ${LIB_DIR}/libmpp.so AND EXISTS ${MPP_INCLUDE_DIR}/vdec/vdec_api.h)
    set(STITCHER_MPP_LIB ${LIB_DIR}/libmpp.so)
    message(STATUS "MPP enabled: ${STITCHER_MPP_LIB}")
endif()

set(TEST_DIR ${CMAKE_SOURCE_DIR}/stitcher_test)
add_executable(planar_stitcher_test ${TEST_DIR}/planar_stitcher_test.cpp)

target_include_directories(planar_stitcher_test PRIVATE
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

target_link_libraries(planar_stitcher_test
    planar_stitcher_core
    ${OpenCV_LIBS}
    ${STITCHER_MPP_LIB}
    ${EGL_LIBRARIES}
    ${GLES_LIBRARIES}
    ${X11_LIBRARIES}
    Threads::Threads
    dl)

# 设置 RPATH，运行时可直接在 lib/ 找到 .so
get_filename_component(_opencv_libdir "${OpenCV_DIR}/../.." ABSOLUTE)
set_target_properties(planar_stitcher_test PROPERTIES
    BUILD_RPATH "${LIB_DIR}:${_opencv_libdir}"
    INSTALL_RPATH "${LIB_DIR}:${_opencv_libdir}")

# 程序从 ../config.json 读取配置，从 stitcher_test/ 读取资源
