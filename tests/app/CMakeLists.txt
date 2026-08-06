# LogFilter 纯函数单元测试
add_etest(
    NAME test_app_log_filter
    SOURCES log_filter_test.cpp
            ${CMAKE_SOURCE_DIR}/src/app/widgets/LogFilter.cpp
    INCLUDES ${CMAKE_SOURCE_DIR}/src/app
    QT_MODULES Core Test
    LABELS APP
)

# RunConfig 格式单测（.erun 模型，依赖 etest_core logger）
add_etest(
    NAME test_app_run_config
    SOURCES run_config_test.cpp
            ${CMAKE_SOURCE_DIR}/src/app/editors/RunConfig.cpp
    INCLUDES ${CMAKE_SOURCE_DIR}/src/app ${CMAKE_SOURCE_DIR}/src
    LIBS etest_core
    QT_MODULES Core Test
    LABELS APP
)
