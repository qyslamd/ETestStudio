set(TARGET_NAME "etest_engine")

add_library(${TARGET_NAME} STATIC
    TestExecutionEngine.cpp
)

target_include_directories(${TARGET_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${TARGET_NAME} PUBLIC
    etest_core
    icd_utility
    spdlog::spdlog
    Qt5::Core
)

set_target_properties(${TARGET_NAME} PROPERTIES
    AUTOMOC ON
)
