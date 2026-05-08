# 配置管理模块测试
add_etest(
    NAME test_core_config
    SOURCES config_test.cpp
    LIBS core
    QT_MODULES Core Test
    LABELS CORE
)

# 日志系统测试
add_etest(
    NAME test_core_logger
    SOURCES logger_test.cpp
    LIBS core
    QT_MODULES Core Test
    LABELS CORE
)

# 崩溃捕获模块测试（仅Windows实现）
if(WIN32)
    add_etest(
        NAME test_core_crashhandler
        SOURCES crashhandler_test.cpp
        LIBS core dbghelp
        QT_MODULES Core Test Widgets
        LABELS CORE
    )
    # 生成disabled崩溃测试脚本
    set(TARGET_NAME test_core_crashhandler)
    configure_file("${CMAKE_SOURCE_DIR}/cmake/run_disabled_test.bat.in"
                   "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/run_${TARGET_NAME}_disabled.bat" @ONLY)
endif()

# 工具类测试
add_etest(
    NAME test_core_utils
    SOURCES utils_test.cpp
    LIBS core
    QT_MODULES Core Test
    LABELS CORE
)

# 字符串工具类测试
add_etest(
    NAME test_core_string_util
    SOURCES string_util_test.cpp
    LIBS core
    QT_MODULES Core Test
    LABELS CORE
)

# 字节处理工具类测试
add_etest(
    NAME test_core_byte_util
    SOURCES byte_util_test.cpp
    LIBS core
    QT_MODULES Core Test
    LABELS CORE
)

# 插件管理器测试
add_etest(
    NAME test_core_plugin_manager
    SOURCES plugin_manager_test.cpp
    LIBS core
    QT_MODULES Core Test Widgets
    LABELS CORE
)

# AD设备插件接口测试
add_etest(
    NAME test_core_ad_device_plugin
    SOURCES ad_device_plugin_test.cpp
    LIBS core
    QT_MODULES Core Test
    LABELS CORE
)

# 项目管理器测试
add_etest(
    NAME test_core_project_manager
    SOURCES project_manager_test.cpp
    LIBS core
    QT_MODULES Core Test
    LABELS CORE
)

# 全局异常处理器测试
add_etest(
    NAME test_core_global_exception_handler
    SOURCES global_exception_handler_test.cpp
    LIBS core
    QT_MODULES Core Test Widgets
    LABELS CORE
)
# 生成disabled崩溃测试脚本
set(TARGET_NAME test_core_global_exception_handler)
configure_file("${CMAKE_SOURCE_DIR}/cmake/run_disabled_test.bat.in"
               "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/run_${TARGET_NAME}_disabled.bat" @ONLY)