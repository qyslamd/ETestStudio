set(TARGET_NAME "wisdom_viewer")

qt5_add_resources(WISDOM_RESOURCES data/poems.qrc)

add_executable(${TARGET_NAME}
    src/main.cpp
    src/WisdomDatabase.cpp
    src/WisdomWidget.cpp
    include/WisdomDatabase.h
    include/WisdomWidget.h
    ${WISDOM_RESOURCES}
)

target_include_directories(${TARGET_NAME} PRIVATE include)
target_link_libraries(${TARGET_NAME} PRIVATE Qt5::Widgets Qt5::Sql)

set_target_properties(${TARGET_NAME} PROPERTIES
    AUTOMOC ON
    AUTORCC ON
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(WIN32)
    configure_file(
        "${CMAKE_SOURCE_DIR}/cmake/run_app.bat.in"
        "${CMAKE_BINARY_DIR}/bin/run_${TARGET_NAME}.bat"
        @ONLY
    )
endif()
