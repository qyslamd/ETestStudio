set(TARGET_NAME "protocal-demo")

if(NOT TARGET etest_protocal)
    add_subdirectory(${CMAKE_SOURCE_DIR}/src/app/protocal ${CMAKE_BINARY_DIR}/protocal_build)
endif()

add_executable(${TARGET_NAME}
    main.cpp
    MainWindow.h
    MainWindow.cpp
)

target_link_libraries(${TARGET_NAME} PRIVATE
    etest_protocal
    Qt5::Widgets
    Qt5::Core
    Qt5::Gui
)

target_include_directories(${TARGET_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src/app  # for editor/IEditor.h
)

set_target_properties(${TARGET_NAME} PROPERTIES
    AUTOMOC ON
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(WIN32)
    configure_file(
        "${CMAKE_SOURCE_DIR}/cmake/run_app.bat.in"
        "${CMAKE_SOURCE_DIR}/scripts/run_${TARGET_NAME}.bat"
        @ONLY
    )
endif()
