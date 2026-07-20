set(TARGET_NAME "test-executor")


add_executable(${TARGET_NAME} WIN32 main.cpp)

target_link_libraries(${TARGET_NAME} PRIVATE
    etest_engine
    etest_core
    etest_core_ui
    etest_ui
    Qt5::Widgets
    Qt5::Core
    Qt5::Gui
    spdlog::spdlog
)

target_include_directories(${TARGET_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

set_target_properties(${TARGET_NAME} PROPERTIES
    AUTOMOC ON
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

configure_run_script("run_${TARGET_NAME}")
