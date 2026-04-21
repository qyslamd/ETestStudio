# 开启AUTOMOC处理Qt元对象
set(CMAKE_AUTOMOC ON)

# 崩溃捕获模块测试（仅Windows实现）
if(WIN32)
    add_executable(test_core_crashhandler crashhandler_test.cpp)
    target_link_libraries(test_core_crashhandler PRIVATE
        core
        gtest
        gtest_main
        Qt5::Core
        Qt5::Widgets
    )
    
    add_test(NAME CrashHandlerBasicTest
        COMMAND test_core_crashhandler
        WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    )
endif()

# 配置管理模块测试
add_executable(test_core_config config_test.cpp)
target_link_libraries(test_core_config PRIVATE
    core
    gtest
    gtest_main
    Qt5::Core
)

add_test(NAME ConfigBasicTest
    COMMAND test_core_config
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)
