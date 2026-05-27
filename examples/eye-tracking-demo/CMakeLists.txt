set(TARGET_NAME "eye-tracking-demo")

add_executable(${TARGET_NAME}
    main.cpp
    ${CMAKE_SOURCE_DIR}/src/app/EyeWidget.h
    ${CMAKE_SOURCE_DIR}/src/app/EyeWidget.cpp
)

target_include_directories(${TARGET_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src/app
)

target_link_libraries(${TARGET_NAME} PRIVATE
    Qt5::Widgets
    Qt5::Core
    Qt5::Gui
)

set_target_properties(${TARGET_NAME} PROPERTIES
    AUTOMOC ON
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
