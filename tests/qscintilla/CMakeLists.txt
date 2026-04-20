cmake_minimum_required(VERSION 3.19)
project(qscintilla_test LANGUAGES CXX)

# 复用项目全局编译配置
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

# 开启Qt AUTOMOC
set(CMAKE_AUTOMOC ON)

# 查找依赖
find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets PrintSupport)

# 测试可执行文件
add_executable(qscintilla_highlight_test test_qscintilla_highlight.cpp)

# 包含目录
target_include_directories(qscintilla_highlight_test PRIVATE
    ${CMAKE_SOURCE_DIR}/3rdparty/QScintilla-2.11.3/Qt4Qt5
    ${CMAKE_SOURCE_DIR}/3rdparty/googletest-1.17.0/googletest/include
    ${CMAKE_SOURCE_DIR}/3rdparty/googletest-1.17.0/googlemock/include
)

# 链接库
target_link_libraries(qscintilla_highlight_test PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::PrintSupport
    qsci
    gtest
    gtest_main
    gmock
)

# 适配MSVC编译选项
if(MSVC)
    target_compile_options(qscintilla_highlight_test PRIVATE /utf-8)
    target_compile_definitions(qscintilla_highlight_test PRIVATE
        UNICODE
        _UNICODE
        _CRT_SECURE_NO_WARNINGS
    )
endif()

# 设置输出路径
set_target_properties(qscintilla_highlight_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# 注册到CTest
add_test(NAME QScintillaHighlightTest 
         COMMAND qscintilla_highlight_test
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
        "${CMAKE_CURRENT_SOURCE_DIR}/run_qscintilla_test.bat.in"
        "${CMAKE_CURRENT_SOURCE_DIR}/run_qscintilla_test.bat"
        @ONLY
    )
endif()