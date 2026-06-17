set(TARGET_NAME "protocol-demo")

if(NOT TARGET etest_protocol)
    add_subdirectory(${CMAKE_SOURCE_DIR}/src/protocol ${CMAKE_BINARY_DIR}/protocol_build)
endif()

add_executable(${TARGET_NAME}
    main.cpp
    ${CMAKE_SOURCE_DIR}/src/app/resource.qrc
)

target_link_libraries(${TARGET_NAME} PRIVATE
    etest_protocol
    Qt5::Widgets
    Qt5::Core
    Qt5::Gui
)

target_include_directories(${TARGET_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src  # for api/IEditor.h
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
