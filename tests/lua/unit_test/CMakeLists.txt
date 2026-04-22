# 测试可执行文件
set(TARGET_NAME "test_lua_cpp_interop")
add_executable(${TARGET_NAME} test_lua_cpp_interop.cpp)
set_target_properties(${TARGET_NAME} PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS ON
    )

# 包含目录
target_include_directories(${TARGET_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/3rdparty/lua-5.4.4/src
    ${CMAKE_SOURCE_DIR}/3rdparty/googletest-1.17.0/googletest/include
    ${CMAKE_SOURCE_DIR}/3rdparty/googletest-1.17.0/googlemock/include
)

# 链接库
target_link_libraries(${TARGET_NAME} PRIVATE
    liblua
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
add_test(NAME LuaCppInteropTest 
         COMMAND ${TARGET_NAME}
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
