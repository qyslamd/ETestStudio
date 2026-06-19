# CMakeLists.txt - 顶层CMake配置文件
# create by zyh on 2026-04-19
# 这个CMakeLists.txt文件是整个项目的入口，负责设置全局配置、查找和编译第三方依赖，以及包含子目录的CMakeLists.txt文件。
# 主要功能包括：
# 1. 设置CMake的最低版本要求和项目基本信息。
# 2. 定义全局的C++标准和输出目录。
# 3. 查找Qt库和其他第三方依赖，并将它们添加到构建中。
# 4. 包含src目录下的CMakeLists.txt文件，编译应用程序。
# 注意：这个文件中的路径和配置可能需要根据实际环境进行调整，例如Qt库的路径和版本号。

cmake_minimum_required(VERSION 3.19)
project(ETestStudio
    VERSION 1.0.0
    DESCRIPTION "ETestStudio - Automated Test System"
    LANGUAGES CXX C)

#=======================================
# 自动解压第三方依赖库
if (EXISTS "cmake/scripts/extract_dependencies.cmake")
    include(cmake/scripts/extract_dependencies.cmake)
endif()


#=======================================
# CMake全局变量
set(CMAKE_CXX_STANDARD 17)  # C++ 标准，影响全局
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
# 设置输出目录,所有target的输出目录都统一到bin和lib下
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

#=======================================
# 添加自己编写的一些cmake模块
include(${CMAKE_SOURCE_DIR}/cmake/debug_helper.cmake)

#=======================================
# 第三方依赖本地源码编译
#=======================================
# 查找Qt库
find_package(Qt5 COMPONENTS Core Gui Widgets PrintSupport Test Xml Svg network Concurrent Sql
    CONFIG REQUIRED)

if(Qt5_FOUND)
    get_target_property(_qt_core_location Qt5::Core LOCATION)
    get_filename_component(QT_LIB_DIR "${_qt_core_location}" DIRECTORY)
    if(WIN32)
        get_filename_component(QT_BIN_DIR "${QT_LIB_DIR}/../bin" ABSOLUTE)
        get_filename_component(QT_PLUGINS_DIR "${QT_BIN_DIR}/../plugins" ABSOLUTE)
    else()
        # TODO：
    endif()
endif()

# Qt-Advanced-Docking-System Qt版本的 dock 组件
set(ADS_VERSION "3.8.3")  # 可以覆盖默认的版本提取方式，直接指定版本号
set(BUILD_STATIC ON CACHE BOOL "Build static libraries for Qt-Advanced-Docking-System" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "Do not build examples for Qt-Advanced-Docking-System" FORCE)  # 不需要编译示例

# 应用Qt-Advanced-Docking-System CMake补丁
include(cmake/qads/patch_cmake_version.cmake)
add_subdirectory(3rdparty/Qt-Advanced-Docking-System-3.8.3)

# QXlsx Excel读写库
add_subdirectory(3rdparty/QXlsx-1.5.0/QXlsx)

#  zlib 压缩库，通常被大多数库依赖，放在最前面编译
set(ZLIB_BUILD_SHARED OFF CACHE BOOL "Do not build shared libraries for zlib" FORCE)
set(ZLIB_BUILD_TESTING OFF CACHE BOOL "Do not build tests for zlib" FORCE)
add_subdirectory(3rdparty/zlib-1.3.2)

# libpng PNG库
# 应用libpng CMake补丁
include(cmake/libpng/patch_cmake_version.cmake)
include(cmake/libpng/patch_msys_env.cmake)
# 由于使用我们自行编译的zlib，它生成的静态库目标是 ZLIB::ZLIBSTATIC 是目标 zlibstatic的别名，
# 而不是 ZLIB::ZLIB 这个名字，因此 libpng 的 CMakeLists.txt 中找不到 ZLIB::ZLIB 这个目标。
# 因此，这里将这个目标改个名字，让libpng能够找到并链接到它
if(NOT TARGET ZLIB::ZLIB AND
 TARGET zlibstatic)
    add_library(ZLIB::ZLIB ALIAS zlibstatic)
else()
    message(FATAL_ERROR "SB")
endif()

set(PNG_STATIC ON CACHE BOOL "Build static libpng" FORCE)
set(PNG_SHARED OFF CACHE BOOL "Do not build shared libpng" FORCE)
set(PNG_TESTS OFF CACHE BOOL "Do not build libpng tests" FORCE)

# 让libpng使用我们自己的zlib目标
set(ZLIB_FOUND TRUE CACHE BOOL "ZLIB found" FORCE)
set(ZLIB_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/3rdparty/zlib-1.3.2" CACHE PATH "ZLIB include dir" FORCE)
set(ZLIB_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/3rdparty/zlib-1.3.2" CACHE PATH "ZLIB include dirs" FORCE)
set(ZLIB_LIBRARY ZLIB::ZLIBSTATIC CACHE FILEPATH "ZLIB library" FORCE)

add_subdirectory(3rdparty/libpng-1.6.43)

# spdlog 日志库
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "Do not build spdlog examples" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "Do not build spdlog tests" FORCE)
add_subdirectory(3rdparty/spdlog-1.17.0)

# QWindowKit - frameless window support library (used by SARibbon)
set(QWINDOWKIT_BUILD_STATIC ON CACHE BOOL "Build static library for QWindowKit" FORCE)
set(QWINDOWKIT_BUILD_EXAMPLES OFF CACHE BOOL "Do not build QWindowKit examples" FORCE)
set(QWINDOWKIT_BUILD_DOCUMENTATIONS OFF CACHE BOOL "Do not build QWindowKit docs" FORCE)
set(QWINDOWKIT_BUILD_QUICK OFF CACHE BOOL "Do not build QWindowKit quick module" FORCE)
set(QWINDOWKIT_BUILD_WIDGETS ON CACHE BOOL "Build QWindowKit widgets module" FORCE)
set(QWINDOWKIT_INSTALL OFF CACHE BOOL "Do not install QWindowKit" FORCE)
add_subdirectory(3rdparty/qwindowkit-1.5.0)

# SARibbon CMake 版本补丁：更新 cmake_minimum_required 避免 deprecation 警告
include(cmake/saribbon/patch_cmake_version.cmake)

# SARibbon QWindowKit 补丁：替换 find_package 为本项目直接目标引用
include(cmake/saribbon/patch_qwindowkit.cmake)

# SARibbon 示例补丁：为每个 example 的 CMakeLists.txt 添加 configure_file
# 必须在 add_subdirectory 之前执行，确保 CMakeLists.txt 已包含 configure_file 调用
include(cmake/saribbon/patch_examples.cmake)

# SARibbon Ribbon 界面控件库
set(SARIBBON_BUILD_STATIC_LIBS ON CACHE BOOL "Build static library for SARibbon" FORCE)
set(SARIBBON_BUILD_EXAMPLES ON CACHE BOOL "Do not build SARibbon examples" FORCE)
set(SARIBBON_USE_FRAMELESS_LIB ON CACHE BOOL "Use QWindowKit for frameless window" FORCE)
set(SARIBBON_INSTALL_IN_CURRENT_DIR OFF CACHE BOOL "Do not install in current dir" FORCE)
add_subdirectory(3rdparty/SARibbon-2.5.7)

# googletest 单元测库
add_subdirectory(3rdparty/googletest-1.17.0)

# lua lua库 通过 CMake 补丁方式集成，生成静态库 + 可执行文件
# 使用 configure_file 动态生成 3rdparty/lua-5.4.4/CMakeLists.txt，以及 wmain.c
set(LUA_CMAKE_FILES_DIR ${CMAKE_SOURCE_DIR}/cmake/lua)  # 指定Lua CMake文件的目录
configure_file(${CMAKE_SOURCE_DIR}/cmake/lua/CMakeLists.txt.in 
               ${CMAKE_SOURCE_DIR}/3rdparty/lua-5.4.4/CMakeLists.txt @ONLY)
configure_file(${CMAKE_SOURCE_DIR}/cmake/lua/wmain.c.in 
               ${CMAKE_SOURCE_DIR}/3rdparty/lua-5.4.4/src/wmain.c COPYONLY)
add_subdirectory(3rdparty/lua-5.4.4)

# libharu PDF库
# 让libharu找到我们自己的zlib和libpng
set(ZLIB_FOUND TRUE CACHE BOOL "ZLIB found" FORCE)
set(ZLIB_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/3rdparty/zlib-1.3.2" CACHE PATH "ZLIB include dir" FORCE)
set(ZLIB_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/3rdparty/zlib-1.3.2" CACHE PATH "ZLIB include dirs" FORCE)
set(ZLIB_LIBRARY zlibstatic CACHE FILEPATH "ZLIB library" FORCE)
set(PNG_FOUND TRUE CACHE BOOL "PNG found" FORCE)
set(PNG_PNG_INCLUDE_DIR 
    "${CMAKE_SOURCE_DIR}/3rdparty/libpng-1.6.43"
    "${CMAKE_BINARY_DIR}/3rdparty/libpng-1.6.43"
    CACHE PATH "PNG include dirs" FORCE)
set(PNG_LIBRARY png_static CACHE FILEPATH "PNG library" FORCE)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static library for libharu" FORCE)
add_subdirectory(3rdparty/libharu-2.4.6)

# QScintilla 高级文本编辑器，补丁模式集成，不修改原源码
set(QSCINTILLA_CMAKE_DIR ${CMAKE_SOURCE_DIR}/cmake/qscintilla)
configure_file(${QSCINTILLA_CMAKE_DIR}/CMakeLists.txt.in
               ${CMAKE_SOURCE_DIR}/3rdparty/QScintilla-2.11.3/CMakeLists.txt @ONLY)
add_subdirectory(3rdparty/QScintilla-2.11.3)

if(NOT TARGET QScintilla AND TARGET qsci)
    add_library(QScintilla ALIAS qsci)
else()
    message(FATAL_ERROR "Failed to create alias for QScintilla target. Please check if the target 'qsci' exists.")
endif()

# sol2 - C++ Lua绑定库，纯头文件库，手动定义标准接口目标（最稳定方案）
add_library(sol2 INTERFACE)
target_include_directories(sol2 INTERFACE ${CMAKE_SOURCE_DIR}/3rdparty/sol2-3.3.0/include)
target_link_libraries(sol2 INTERFACE liblua)
add_library(sol2::sol2 ALIAS sol2)

# =======================================
# icd_utility 三方依赖
# =======================================

# pugixml XML 解析库
add_subdirectory(3rdparty/pugixml-1.15)

# nlohmann/json (header-only) — code includes <nlohmann/json.hpp>, need ./3rdparty/ as root
add_library(nlohmann INTERFACE)
target_include_directories(nlohmann INTERFACE ${CMAKE_SOURCE_DIR}/3rdparty)

# tl/expected (header-only) — code includes <tl/expected.hpp>, need ./3rdparty/ as root
add_library(tl INTERFACE)
target_include_directories(tl INTERFACE ${CMAKE_SOURCE_DIR}/3rdparty)

#=======================================
# 设置编译选项，特别是Windows平台下的Unicode支持
# 对于MSVC编译器，启用UTF-8编码支持，并定义Unicode相关的宏，以确保应用程序能够正确处理Unicode字符。
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

# =======================================
# 包含src目录下的CMakeLists.txt，编译应用程序
add_subdirectory(src)

# =======================================
# 包含单元测试
option(BUILD_TESTING "Build unit tests" ON)
if (BUILD_TESTING)
    include(CTest)
    enable_testing()
    add_subdirectory(tests)
endif()

# =======================================
# 包含示例插件
option(BUILD_ETEST_EXAMPLES "Build etest example plugins" ON)
if (BUILD_ETEST_EXAMPLES)
    add_subdirectory(examples/plugins/hello_plugin)
    add_subdirectory(examples/plugins/mock_ad_device)
    add_subdirectory(examples/plugins/mock_da_device)
    add_subdirectory(examples/plugins/mock_serial_device)
    add_subdirectory(examples/plugins/mock_a429_device)
    add_subdirectory(examples/plugins/mock_can_device)
    add_subdirectory(examples/eye-tracking-demo)
    add_subdirectory(examples/guidance-demo)
    add_subdirectory(examples/lua-debugger-demo)
    add_subdirectory(examples/topology-demo)
    add_subdirectory(examples/protocol-demo)
    add_subdirectory(examples/testprogram-demo)
endif()


# =======================================
# 生成windows下的clang-format脚本
if(WIN32)
    # 找到 clang-format 可执行文件路径，生成 run_clang_format.bat 脚本 
    find_program(CLANG_FORMAT_EXECUTABLE 
        NAMES clang-format clang-format-15 clang-format-14 clang-format-13 clang-format-12
        HINTS $ENV{VSINSTALLDIR}/VC/Tools/Llvm/bin
              $ENV{ProgramFiles}/LLVM/bin
        )
    if(NOT CLANG_FORMAT_EXECUTABLE)
        message(WARNING "clang-format executable not found. Please install clang-format and ensure it's in your PATH.")
    else()
        configure_file(
            "${CMAKE_SOURCE_DIR}/cmake/format_code.bat.in"
            "${CMAKE_SOURCE_DIR}/scripts/format_code.bat"
            @ONLY
        )
    endif()

    # 找到 cloc 可执行文件路径（用于统计代码行数），生成 code_stats.bat 脚本
    find_program(CLOC_EXECUTABLE
        NAMES cloc cloc-2.06
        HINTS ${CMAKE_SOURCE_DIR}/3rdparty/cloc-2.06
    )
    if(NOT CLOC_EXECUTABLE)
        message(WARNING "cloc executable not found! Put cloc-2.06.exe in 3rdparty/cloc-2.06/")
    else()
        configure_file(
            "${CMAKE_SOURCE_DIR}/cmake/code_stats.bat.in"
            "${CMAKE_SOURCE_DIR}/scripts/code_stats.bat"
            @ONLY
        )
        configure_file(
            "${CMAKE_SOURCE_DIR}/cmake/code_stats.ps1.in"
            "${CMAKE_SOURCE_DIR}/scripts/code_stats.ps1"
            @ONLY
        )
    endif()
endif()
