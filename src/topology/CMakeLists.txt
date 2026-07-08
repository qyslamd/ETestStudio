set(TARGET_NAME etest_topology)
add_library(${TARGET_NAME} STATIC
    TopologyBlockItem.h TopologyBlockItem.cpp
    TopologyOutlineWidget.h TopologyOutlineWidget.cpp
    TopologyPathRouter.h TopologyPathRouter.cpp
    TopologyDocument.h TopologyDocument.cpp
    TopologyJsonSerializer.h TopologyJsonSerializer.cpp
    topology_items.h topology_items.cpp
    TopologyScene.h TopologyScene.cpp
    TopologyView.h TopologyView.cpp
    TopologyEditorWidget.h TopologyEditorWidget.cpp
    PropertyPanelWidget.h PropertyPanelWidget.cpp
    ComboBoxDelegate.h ComboBoxDelegate.cpp
    DeviceTemplateManager.h DeviceTemplateManager.cpp
    DevicePaletteWidget.h DevicePaletteWidget.cpp
    UndoCommands.h UndoCommands.cpp
    TopologyTheme.h TopologyTheme.cpp
    ConnectionCleanup.h ConnectionCleanup.cpp
    TopologySceneRenderer.h TopologySceneRenderer.cpp
    TopologyExportController.h TopologyExportController.cpp
    TopologyCleanupController.h TopologyCleanupController.cpp
    DevicePortBindingDialog.h DevicePortBindingDialog.cpp
)
target_include_directories(${TARGET_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)
set_target_properties(${TARGET_NAME} PROPERTIES AUTOMOC ON)
target_link_libraries(${TARGET_NAME} PUBLIC Qt5::Widgets Qt5::Core Qt5::Gui Qt5::Concurrent Qt5::Svg etest_api etest_core etest_ui)
