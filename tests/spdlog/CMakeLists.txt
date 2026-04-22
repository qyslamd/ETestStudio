add_etest(
    NAME test_spdlog_basic
    SOURCES test_spdlog_basic.cpp
    LIBS spdlog::spdlog
    INCLUDES ${CMAKE_SOURCE_DIR}/3rdparty/spdlog-1.17.0/include
    LABELS THIRD_PARTY
)
