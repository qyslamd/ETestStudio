# 配置管理模块测试
add_etest(
    NAME test_core_config
    SOURCES config_test.cpp
    LIBS core
    QT_MODULES Core
    LABELS CORE
)

# 日志系统测试
add_etest(
    NAME test_core_logger
    SOURCES logger_test.cpp
    LIBS core
    QT_MODULES Core
    LABELS CORE
)

# 崩溃捕获模块测试（仅Windows实现）
if(WIN32)
    add_etest(
        NAME test_core_crashhandler
        SOURCES crashhandler_test.cpp
        LIBS core dbghelp
        QT_MODULES Core Widgets
        LABELS CORE
    )
endif()

# 工具类测试
add_etest(
    NAME test_core_utils
    SOURCES utils_test.cpp
    LIBS core
    QT_MODULES Core
    LABELS CORE
)

# 字符串工具类测试
add_etest(
    NAME test_core_string_util
    SOURCES string_util_test.cpp
    LIBS core
    QT_MODULES Core
    LABELS CORE
)

# 字节处理工具类测试
add_etest(
    NAME test_core_byte_util
    SOURCES byte_util_test.cpp
    LIBS core
    QT_MODULES Core
    LABELS CORE
)