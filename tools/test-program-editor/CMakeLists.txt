set(TARGET_NAME "test-program-editor")

if(NOT TARGET etest_core)
    add_subdirectory(${CMAKE_SOURCE_DIR}/src/core ${CMAKE_BINARY_DIR}/core_build)
endif()
if(NOT TARGET etest_ui)
    add_subdirectory(${CMAKE_SOURCE_DIR}/src/libui ${CMAKE_BINARY_DIR}/libui_build)
endif()
if(NOT TARGET etest_program)
    add_subdirectory(${CMAKE_SOURCE_DIR}/src/test_program ${CMAKE_BINARY_DIR}/test_program_build)
endif()

add_executable(${TARGET_NAME}
    main.cpp
)

target_link_libraries(${TARGET_NAME} PRIVATE
    Qt5::Widgets
    Qt5::Core
    Qt5::Gui
    etest_core
    etest_program
    etest_ui
    spdlog::spdlog
)

target_include_directories(${TARGET_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/libui
    ${CMAKE_SOURCE_DIR}/3rdparty/spdlog-1.17.0/include
)

set_target_properties(${TARGET_NAME} PROPERTIES
    AUTOMOC ON
    AUTORCC ON
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(WIN32)
    configure_file(
        "${CMAKE_SOURCE_DIR}/cmake/run_app.bat.in"
        "${CMAKE_BINARY_DIR}/bin/run_${TARGET_NAME}.bat"
        @ONLY
    )
endif()
