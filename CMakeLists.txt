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
# FetchContent 第三方依赖配置
include(FetchContent)
set(FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/3rdparty" CACHE PATH "第三方依赖下载目录" FORCE)

# 1. spdlog 日志库
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.12.0
    GIT_SHALLOW ON
)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(spdlog)

# 2. gtest 单元测试框架
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG release-1.12.1
    GIT_SHALLOW ON
)
set(BUILD_GMOCK ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# 3. Lua 脚本引擎
FetchContent_Declare(
    lua
    GIT_REPOSITORY https://github.com/lua/lua.git
    GIT_TAG v5.4.4
    GIT_SHALLOW ON
)
if(NOT lua_POPULATED)
    FetchContent_Populate(lua)
    file(GLOB LUA_SOURCES "${lua_SOURCE_DIR}/*.c")
    list(REMOVE_ITEM LUA_SOURCES "${lua_SOURCE_DIR}/lua.c" "${lua_SOURCE_DIR}/luac.c")
    add_library(lua STATIC ${LUA_SOURCES})
    target_include_directories(lua PUBLIC ${lua_SOURCE_DIR})
endif()

# 4. libharu PDF库
FetchContent_Declare(
    libharu
    GIT_REPOSITORY https://github.com/libharu/libharu.git
    GIT_TAG v2.4.3
    GIT_SHALLOW ON
)
set(LIBHPDF_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LIBHPDF_BUILD_SHARED OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(libharu)

# 5. Qt Advanced Docking System
FetchContent_Declare(
    qads
    GIT_REPOSITORY https://github.com/githubuser0xFFFF/Qt-Advanced-Docking-System.git
    GIT_TAG 3.8.3
    GIT_SHALLOW ON
)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(qads)

# 6. QScintilla 文本编辑器
FetchContent_Declare(
    qscintilla
    GIT_REPOSITORY https://github.com/opencor/qscintilla.git
    GIT_TAG v2.11.6
    GIT_SHALLOW ON
)
if(NOT qscintilla_POPULATED)
    FetchContent_Populate(qscintilla)
    add_subdirectory("${qscintilla_SOURCE_DIR}/Qt4Qt5" ${qscintilla_BINARY_DIR})
endif()

# 7. Qt Xlsx Excel库
FetchContent_Declare(
    qtxlsx
    GIT_REPOSITORY https://github.com/dbzhang800/QtXlsxWriter.git
    GIT_TAG v0.3.0
    GIT_SHALLOW ON
)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(qtxlsx)

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
