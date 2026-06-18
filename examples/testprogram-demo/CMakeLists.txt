set(TARGET_NAME "testprogram-demo")

add_executable(${TARGET_NAME}
    main.cpp
    MainWindow.h
    MainWindow.cpp
    ${CMAKE_SOURCE_DIR}/src/app/TestProgramData.h
    ${CMAKE_SOURCE_DIR}/src/app/TestProgramData.cpp
    ${CMAKE_SOURCE_DIR}/src/app/editors/TestProgramEditorWidget.h
    ${CMAKE_SOURCE_DIR}/src/app/editors/TestProgramEditorWidget.cpp
)

target_link_libraries(${TARGET_NAME} PRIVATE
    Qt5::Widgets
    Qt5::Core
    Qt5::Gui
    etest_core
    spdlog::spdlog
)

target_include_directories(${TARGET_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/app
    ${CMAKE_SOURCE_DIR}/src/core
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
