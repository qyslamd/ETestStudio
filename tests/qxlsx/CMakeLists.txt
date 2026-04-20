cmake_minimum_required(VERSION 3.19)
project(qxlsx_test LANGUAGES CXX)

# 复用全局编译配置
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

# 开启Qt AUTOMOC
set(CMAKE_AUTOMOC ON)

# 查找依赖
if(TARGET Qt5::Core AND TARGET Qt5::Gui)
    message(STATUS "Qt5 Core and Gui found")
else()
    find_package(Qt5 COMPONENTS Core Gui REQUIRED)
endif()

# 测试可执行文件
add_executable(test_qxlsx_basic test_qxlsx_basic.cpp)

# 链接库
target_link_libraries(test_qxlsx_basic PRIVATE
    Qt5::Core
    Qt5::Gui
    QXlsx::QXlsx
    gtest
    gtest_main
    gmock
)

# INTERFACE_INCLUDE_DIRECTORIES
# INCLUDE_DIRECTORIES
# print_target_properry(QXlsx::QXlsx INCLUDE_DIRECTORIES)
# message(FATAL_ERROR "SB")

# 适配MSVC编译选项
if(MSVC)
    target_compile_options(test_qxlsx_basic PRIVATE /utf-8)
    target_compile_definitions(test_qxlsx_basic PRIVATE
        UNICODE
        _UNICODE
        _CRT_SECURE_NO_WARNINGS
    )
endif()

# 设置输出路径
set_target_properties(test_qxlsx_basic PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# 注册到CTest
add_test(NAME QXlsxBasicTest 
         COMMAND test_qxlsx_basic
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

#=======================================
# 生成运行脚本
set(QT_ROOT_DIR "${Qt5_DIR}/../../..")

if(WIN32)
    set(QT_BIN_DIR "${QT_ROOT_DIR}/bin")
    set(QT_PLUGINS_DIR "${QT_ROOT_DIR}/plugins")

    if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        set(BUILD_TYPE "ninja-debug")
    else()
        set(BUILD_TYPE "ninja-release")
    endif()

    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/run_qxlsx_test.bat.in"
        "${CMAKE_CURRENT_SOURCE_DIR}/run_qxlsx_test.bat"
        @ONLY
    )
endif()
