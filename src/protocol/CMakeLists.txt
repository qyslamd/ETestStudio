set(TARGET_NAME "etest_protocol")

add_library(${TARGET_NAME} STATIC
    ProtocolEditorWidget.h
    ProtocolEditorWidget.cpp
    IcdNodeTreeWidget.h
    IcdNodeTreeWidget.cpp
    IcdBitLayoutView.h
    IcdBitLayoutView.cpp
    IcdPropertyPanel.h
    IcdPropertyPanel.cpp
    IcdProtocolUtils.h
    IcdProtocolUtils.cpp
    IcdFramePreview.h
    IcdFramePreview.cpp
    IcdFramePreviewPanel.h
    IcdFramePreviewPanel.cpp
)

target_include_directories(${TARGET_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

set_target_properties(${TARGET_NAME} PROPERTIES AUTOMOC ON)
target_link_libraries(${TARGET_NAME} PUBLIC Qt5::Widgets Qt5::Core Qt5::Gui Qt5::Concurrent etest_api etest_core etest_core_ui icd_utility etest_ui)
