set(TARGET_NAME "topology-demo")

add_executable(${TARGET_NAME}
    main.cpp
    TopologyDocument.h
    TopologyDocument.cpp
    TopologyJsonSerializer.h
    TopologyJsonSerializer.cpp
    topology_items.h
    topology_items.cpp
    TopologyScene.h
    TopologyScene.cpp
    TopologyView.h
    TopologyView.cpp
    TopologyEditorWidget.h
    TopologyEditorWidget.cpp
    PropertyPanelWidget.h
    PropertyPanelWidget.cpp
    DeviceTemplateManager.h
    DeviceTemplateManager.cpp
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
        "${CMAKE_SOURCE_DIR}/scripts/run_${TARGET_NAME}.bat"
        @ONLY
    )
endif()
