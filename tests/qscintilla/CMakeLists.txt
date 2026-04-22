# 测试可执行文件
set(TARGET_NAME "qscintilla_highlight_test")
add_executable(${TARGET_NAME} test_qscintilla_highlight.cpp)
set_target_properties(${TARGET_NAME} PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS ON
        AUTOMOC ON
    )

# 包含目录
target_include_directories(${TARGET_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/3rdparty/QScintilla-2.11.3/Qt4Qt5
    ${CMAKE_SOURCE_DIR}/3rdparty/googletest-1.17.0/googletest/include
    ${CMAKE_SOURCE_DIR}/3rdparty/googletest-1.17.0/googlemock/include
)

# 链接库
target_link_libraries(${TARGET_NAME} PRIVATE
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
    target_compile_options(${TARGET_NAME} PRIVATE /utf-8)
    target_compile_definitions(${TARGET_NAME} PRIVATE
        UNICODE
        _UNICODE
        _CRT_SECURE_NO_WARNINGS
    )
endif()

# =======================================
# 注册到CTest
add_test(NAME QScintillaHighlightTest 
         COMMAND ${TARGET_NAME}
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# =======================================
# 设置 Qt 的环境变量
# cmake函数 set_test_env_for_dir 在上一层定义 
set_test_env_for_dir(${CMAKE_CURRENT_SOURCE_DIR})

#=======================================
# 生成运行脚本，不依赖于CTest，直接双击运行
if(WIN32)
    # QT_BIN_DIR 和 QT_PLUGINS_DIR 在顶层CMakeLists.txt中设置
    configure_file(
        "${CMAKE_SOURCE_DIR}/cmake/run_app.bat.in"
        "${CMAKE_CURRENT_SOURCE_DIR}/run_test.bat"
        @ONLY
    )
endif()