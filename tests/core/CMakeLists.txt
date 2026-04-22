# 崩溃捕获模块测试（仅Windows实现）
if(WIN32)
    set(TARGET_NAME "test_core_crashhandler")
    add_executable(${TARGET_NAME} crashhandler_test.cpp)
    set_target_properties(${TARGET_NAME} PROPERTIES
        AUTOMOC ON
    )
    target_link_libraries(${TARGET_NAME} PRIVATE
        core
        gtest
        gtest_main
        Qt5::Core
        Qt5::Widgets
    )
    
    add_test(NAME CrashHandlerBasicTest
        COMMAND ${TARGET_NAME}
        WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    )
endif()

# 配置管理模块测试
set(TARGET_NAME "test_core_config")
add_executable(${TARGET_NAME} config_test.cpp)
target_link_libraries(${TARGET_NAME} PRIVATE
    core
    gtest
    gtest_main
    Qt5::Core
)
set_target_properties(${TARGET_NAME} PROPERTIES
    AUTOMOC ON
)

# =======================
# 添加到CTest管理
add_test(NAME ConfigBasicTest
    COMMAND ${TARGET_NAME}
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)
# =======================
# 设置 Qt 的环境变量
# cmake函数 set_test_env_for_dir 在上一层定义 
set_test_env_for_dir(${CMAKE_CURRENT_SOURCE_DIR})