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
