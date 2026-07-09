set(TARGET_NAME "topology-editor")

if(NOT TARGET etest_ui)
    add_subdirectory(${CMAKE_SOURCE_DIR}/src/libui ${CMAKE_BINARY_DIR}/libui_build)
endif()
if(NOT TARGET etest_topology)
    add_subdirectory(${CMAKE_SOURCE_DIR}/src/topology ${CMAKE_BINARY_DIR}/topology_build)
endif()

add_executable(${TARGET_NAME}
    main.cpp
    ${CMAKE_SOURCE_DIR}/src/app/resource.qrc
)

target_link_libraries(${TARGET_NAME} PRIVATE
    etest_topology
    etest_core_ui
    etest_ui
    Qt5::Widgets
    Qt5::Core
    Qt5::Gui
)

target_include_directories(${TARGET_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
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
