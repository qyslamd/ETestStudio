# RunConfig 格式单测（.erun 模型，依赖 etest_core logger）
add_etest(
    NAME test_runconfig_run_config
    SOURCES run_config_test.cpp
            ${CMAKE_SOURCE_DIR}/src/runconfig/RunConfig.cpp
    INCLUDES ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/runconfig
    LIBS etest_core
    QT_MODULES Core Test
    LABELS RUNCONFIG
)
