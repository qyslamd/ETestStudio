# 配置管理模块测试
add_etest(
    NAME test_core_config
    SOURCES config_test.cpp
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