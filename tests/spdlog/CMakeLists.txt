cmake_minimum_required(VERSION 3.19)
project(spdlog_test LANGUAGES CXX)

# 复用全局编译配置
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

# 测试可执行文件
add_executable(test_spdlog_basic test_spdlog_basic.cpp)

# 包含目录
target_include_directories(test_spdlog_basic PRIVATE
    ${CMAKE_SOURCE_DIR}/3rdparty/spdlog-1.17.0/include
    ${CMAKE_SOURCE_DIR}/3rdparty/googletest-1.17.0/googletest/include
    ${CMAKE_SOURCE_DIR}/3rdparty/googletest-1.17.0/googlemock/include
)

# 链接库
target_link_libraries(test_spdlog_basic PRIVATE
    spdlog::spdlog
    gtest
    gtest_main
    gmock
)

# 适配MSVC编译选项
if(MSVC)
    target_compile_options(test_spdlog_basic PRIVATE /utf-8)
    target_compile_definitions(test_spdlog_basic PRIVATE
        UNICODE
        _UNICODE
        _CRT_SECURE_NO_WARNINGS
    )
endif()

# 设置输出路径
set_target_properties(test_spdlog_basic PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# 注册到CTest
add_test(NAME SpdlogBasicTest 
         COMMAND test_spdlog_basic
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
