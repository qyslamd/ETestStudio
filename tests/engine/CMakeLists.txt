# SignalResolver 单元测试
add_etest(
    NAME test_engine_signal_resolver
    SOURCES signal_resolver_test.cpp
    LIBS etest_engine etest_core icd_utility
    QT_MODULES Core Test
    LABELS ENGINE
)

# SignalCodec 单元测试
add_etest(
    NAME test_engine_signal_codec
    SOURCES signal_codec_test.cpp
    LIBS etest_engine etest_core icd_utility
    QT_MODULES Core Test
    LABELS ENGINE
)

# HardwareManager 单元测试
add_etest(
    NAME test_engine_hardware_manager
    SOURCES hardware_manager_test.cpp
    LIBS etest_engine etest_core icd_utility
    QT_MODULES Core Test
    LABELS ENGINE
)

# StepRunner 单元测试
add_etest(
    NAME test_engine_step_runner
    SOURCES step_runner_test.cpp
    LIBS etest_engine etest_core icd_utility
    QT_MODULES Core Test
    LABELS ENGINE
)
