# 测试可执行文件
set(TARGET_NAME "test_qxlsx_basic")
add_executable(${TARGET_NAME} test_qxlsx_basic.cpp)

set_target_properties(${TARGET_NAME} PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS ON
        AUTOMOC ON
    )

# 链接库
target_link_libraries(${TARGET_NAME} PRIVATE
    Qt5::Core
    Qt5::Gui
    QXlsx::QXlsx
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
add_test(NAME QXlsxBasicTest 
         COMMAND ${TARGET_NAME}
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
