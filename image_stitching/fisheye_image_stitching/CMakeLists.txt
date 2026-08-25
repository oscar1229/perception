cmake_minimum_required(VERSION 3.10)
project(sv_avm_demo)

# 本工程仅依赖预编译库(lib/*.so)与安装头文件(include/)，不包含 sv_avm 源码。
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -fPIC -fpermissive -w")

# 需与构建 .so 时保持一致：display.cpp 中的 X11/FB 分支由 EGL_USE_X11 决定
add_definitions(
    -DGLOG_USE_GLOG_EXPORT
    -DEGL_USE_X11
    -DLINUX
    -DSV_STONKAM_TEST
    -DSTTONKAM_DEBUG
    -DNDEBUG
    -DUBUNTU
)

find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs highgui calib3d)
find_package(glog QUIET)
find_package(PkgConfig REQUIRED)
pkg_check_modules(EGL REQUIRED egl)
pkg_check_modules(GLES REQUIRED glesv2)
find_package(X11 REQUIRED)
find_package(Threads REQUIRED)

# MPP 摄像头输入：从本包的 lib/ 和 include/mpp/ 取，无需外部路径。
# mpp 已由 sv_avm make install 一并安装到这里。
set(SV_AVM_MPP_LIB "")
if(EXISTS ${CMAKE_SOURCE_DIR}/lib/libmpp.so AND
   EXISTS ${CMAKE_SOURCE_DIR}/include/mpp/sys/sys_api.h)
    add_definitions(-DSV_AVM_ENABLE_MPP)
    set(SV_AVM_MPP_LIB ${CMAKE_SOURCE_DIR}/lib/libmpp.so)
    set(SV_AVM_MPP_INCLUDE_DIRS
        ${CMAKE_SOURCE_DIR}/include/mpp/sys
        ${CMAKE_SOURCE_DIR}/include/mpp/vi
        ${CMAKE_SOURCE_DIR}/include/mpp/v2d
        ${CMAKE_SOURCE_DIR}/include/mpp/venc
        ${CMAKE_SOURCE_DIR}/include/mpp/vdec
        ${CMAKE_SOURCE_DIR}/include/mpp/utils
        ${CMAKE_SOURCE_DIR}/include/mpp/al
    )
    message(STATUS "MPP live VI input enabled (from install tree)")
else()
    message(WARNING "MPP not found in lib/ or include/mpp/; camera input unavailable, will use fallback images.")
endif()

# 安装头文件保持了原工程的目录结构，因此以 include/ 为根即可满足
# #include "src/svrender/..." / "common/svtype.hpp" 等写法
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
    ${EGL_INCLUDE_DIRS}
    ${GLES_INCLUDE_DIRS}
    ${X11_INCLUDE_DIR}
    ${SV_AVM_MPP_INCLUDE_DIRS}
)

# --- 引入预编译库 ---
set(SV_AVM_LIB_DIR ${CMAKE_SOURCE_DIR}/lib)
foreach(_lib svmparam svmcalibrate svrender)
    add_library(${_lib} SHARED IMPORTED)
    set_target_properties(${_lib} PROPERTIES
        IMPORTED_LOCATION ${SV_AVM_LIB_DIR}/lib${_lib}.so)
endforeach()

set(TEST_DIR ${CMAKE_SOURCE_DIR}/sv_avm_test)
add_executable(sv_avm_render_test ${TEST_DIR}/sv_avm_render_main_test.cpp)

target_link_libraries(sv_avm_render_test
    svrender
    svmcalibrate
    svmparam
    ${OpenCV_LIBS}
    glog::glog
    ${GLES_LIBRARIES} ${EGL_LIBRARIES}
    ${X11_LIBRARIES}
    Threads::Threads
    dl assimp
    ${SV_AVM_MPP_LIB}
)

# 运行时可直接在 lib/ 找到 .so，无需设置 LD_LIBRARY_PATH。
# OpenCV 在非系统路径，exe 直接链接了 OpenCV 模块，RPATH 也需要包含 OpenCV 路径。
get_filename_component(_opencv_libdir "${OpenCV_DIR}/../.." ABSOLUTE)
set_target_properties(sv_avm_render_test PROPERTIES
    BUILD_RPATH "${SV_AVM_LIB_DIR}:${_opencv_libdir}"
    INSTALL_RPATH "${SV_AVM_LIB_DIR}:${_opencv_libdir}")

# 程序直接从 ../sv_avm_test/ 读取 res/ 和 _aParam.xml，无需拷贝到 build/。
# config.json 以工程根目录 ../config.json 为唯一配置来源。
