cmake_minimum_required(VERSION 3.19)
project(lua_cpp_interop_test LANGUAGES CXX)

# 复用全局编译配置
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

# 测试可执行文件
add_executable(test_lua_cpp_interop test_lua_cpp_interop.cpp)

# 包含目录
target_include_directories(test_lua_cpp_interop PRIVATE
    ${CMAKE_SOURCE_DIR}/3rdparty/lua-5.4.4/src
    ${CMAKE_SOURCE_DIR}/3rdparty/googletest-1.17.0/googletest/include
    ${CMAKE_SOURCE_DIR}/3rdparty/googletest-1.17.0/googlemock/include
)

# 链接库
target_link_libraries(test_lua_cpp_interop PRIVATE
    liblua
    gtest
    gtest_main
    gmock
)

# 适配MSVC编译选项
if(MSVC)
    target_compile_options(test_lua_cpp_interop PRIVATE /utf-8)
    target_compile_definitions(test_lua_cpp_interop PRIVATE
        UNICODE
        _UNICODE
        _CRT_SECURE_NO_WARNINGS
    )
endif()

# 设置输出路径
set_target_properties(test_lua_cpp_interop PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# 注册到CTest
add_test(NAME LuaCppInteropTest 
         COMMAND test_lua_cpp_interop
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
        "${CMAKE_CURRENT_SOURCE_DIR}/run_lua_unit_test.bat.in"
        "${CMAKE_CURRENT_SOURCE_DIR}/run_lua_unit_test.bat"
        @ONLY
    )
endif()
