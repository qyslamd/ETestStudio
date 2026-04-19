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
project(etest_demo
    VERSION 1.0.0
    DESCRIPTION "Use Qt to create a app like ETest"
    LANGUAGES CXX)

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
# 第三方依赖本地源码编译
#=======================================
# 查找Qt库
find_package(Qt5 COMPONENTS Core Gui Widgets REQUIRED)

# Qt-Advanced-Docking-System Qt版本的 ribbon和dock组件
set(ADS_VERSION "3.8.3")  # 可以覆盖默认的版本提取方式，直接指定版本号
set(BUILD_STATIC ON CACHE BOOL "Build static libraries for Qt-Advanced-Docking-System" FORCE)
add_subdirectory(3rdparty/Qt-Advanced-Docking-System-3.8.3)

# QXlsx Excel读写库
add_subdirectory(3rdparty/QXlsx-1.5.0/QXlsx)

#  zlib 压缩库，通常被大多数库依赖，放在最前面编译
set(ZLIB_BUILD_SHARED OFF CACHE BOOL "Do not build shared libraries for zlib" FORCE)
add_subdirectory(3rdparty/zlib-1.3.2)

# spdlog 日志库
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "Do not build spdlog examples" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "Do not build spdlog tests" FORCE)
add_subdirectory(3rdparty/spdlog-1.17.0)

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
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static library for libharu" FORCE)
add_subdirectory(3rdparty/libharu-2.4.6)

# QScintilla 高级文本编辑器，补丁模式集成，不修改原源码
set(QSCINTILLA_CMAKE_DIR ${CMAKE_SOURCE_DIR}/cmake/qscintilla)
configure_file(${QSCINTILLA_CMAKE_DIR}/CMakeLists.txt.in
               ${CMAKE_SOURCE_DIR}/3rdparty/QScintilla-2.11.3/CMakeLists.txt @ONLY)
add_subdirectory(3rdparty/QScintilla-2.11.3)

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

# 包含QScintilla单元测试
add_subdirectory(tests/qscintilla)

include(CTest)
if (BUILD_TESTING)
  enable_testing()
  # Lua smoke tests: verify lua.exe and luaw.exe basic functionality
  add_test(NAME LuaSmokeBasic
           COMMAND ${CMAKE_COMMAND} -E chdir "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}" lua.exe -e "print('Lua smoke ok')")
  add_test(NAME LuaSmokeNoConsole
           COMMAND ${CMAKE_COMMAND} -E chdir "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}" luaw.exe -e "print('Lua smoke no-console')")
  add_test(NAME LuaSmokeBytecode
           COMMAND cmd /C "${CMAKE_SOURCE_DIR}/lua_smoke_bytecode.bat")
endif()
