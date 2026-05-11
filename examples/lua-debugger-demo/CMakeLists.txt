set(TARGET_NAME "lua-debugger-demo")

add_executable(${TARGET_NAME}
    main.cpp
    LuaDebugger.h
    LuaDebugger.cpp
    LuaEditor.h
    LuaEditor.cpp
    MainWindow.h
    MainWindow.cpp
)

target_link_libraries(${TARGET_NAME} PRIVATE
    Qt5::Widgets
    Qt5::Core
    Qt5::Gui
    QScintilla
    sol2::sol2
    liblua
)

target_include_directories(${TARGET_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/3rdparty/lua-5.4.4/src
)

set_target_properties(${TARGET_NAME} PROPERTIES
    AUTOMOC ON
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(WIN32)
    configure_file(
        "${CMAKE_SOURCE_DIR}/cmake/run_app.bat.in"
        "${CMAKE_SOURCE_DIR}/run_${TARGET_NAME}.bat"
        @ONLY
    )
endif()
