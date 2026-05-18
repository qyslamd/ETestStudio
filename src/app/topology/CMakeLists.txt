set(TARGET_NAME etest_topology)
add_library(${TARGET_NAME} STATIC
    TopologyDocument.h TopologyDocument.cpp
    TopologyJsonSerializer.h TopologyJsonSerializer.cpp
    topology_items.h topology_items.cpp
    TopologyScene.h TopologyScene.cpp
    TopologyView.h TopologyView.cpp
    TopologyEditorWidget.h TopologyEditorWidget.cpp
    PropertyPanelWidget.h PropertyPanelWidget.cpp
    DeviceTemplateManager.h DeviceTemplateManager.cpp
    UndoCommands.h UndoCommands.cpp
    TopologyTheme.h TopologyTheme.cpp
)
target_include_directories(${TARGET_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..  # for editor/IEditor.h
)
set_target_properties(${TARGET_NAME} PROPERTIES AUTOMOC ON)
target_link_libraries(${TARGET_NAME} PUBLIC Qt5::Widgets Qt5::Core Qt5::Gui)
