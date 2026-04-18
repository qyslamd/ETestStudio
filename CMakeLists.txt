cmake_minimum_required(VERSION 3.15)
project(etest_demo
    VERSION 1.0.0
    DESCRIPTION "Use Qt to create a app like ETest"
    LANGUAGES CXX)

#=======================================
# CMake全局变量
set(CMAKE_CXX_STANDARD 17)  # C++ 标准，影响全局
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

#=======================================
# 第三方依赖本地源码编译
# 1. spdlog 日志库
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(3rdparty/spdlog)

if(MSVC)
    add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
    add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
    # 设置Unicode字符集
    add_compile_definitions(
        UNICODE
        _UNICODE
        _CRT_SECURE_NO_WARNINGS
    )
endif()

add_subdirectory(src)
