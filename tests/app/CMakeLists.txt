# LogFilter 纯函数单元测试
add_etest(
    NAME test_app_log_filter
    SOURCES log_filter_test.cpp
            ${CMAKE_SOURCE_DIR}/src/app/widgets/LogFilter.cpp
    INCLUDES ${CMAKE_SOURCE_DIR}/src/app
    QT_MODULES Core Test
    LABELS APP
)
