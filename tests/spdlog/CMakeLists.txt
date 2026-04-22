# 测试可执行文件
set(TARGET_NAME "test_spdlog_basic")
add_executable(${TARGET_NAME} test_spdlog_basic.cpp)
set_target_properties(${TARGET_NAME} PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS ON
    )

# 包含目录
target_include_directories(${TARGET_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/3rdparty/spdlog-1.17.0/include
    ${CMAKE_SOURCE_DIR}/3rdparty/googletest-1.17.0/googletest/include
    ${CMAKE_SOURCE_DIR}/3rdparty/googletest-1.17.0/googlemock/include
)

# 链接库
target_link_libraries(${TARGET_NAME} PRIVATE
    spdlog::spdlog
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

# 注册到CTest
add_test(NAME SpdlogBasicTest 
         COMMAND ${TARGET_NAME}
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

#=======================================
# 生成运行脚本
if(WIN32)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        set(BUILD_TYPE "ninja-debug")
    else()
        set(BUILD_TYPE "ninja-release")
    endif()

    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/run_spdlog_test.bat.in"
        "${CMAKE_CURRENT_SOURCE_DIR}/run_spdlog_test.bat"
        @ONLY
    )
endif()
