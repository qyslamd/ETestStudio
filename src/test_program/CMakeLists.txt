set(TARGET_NAME "etest_program")

add_library(${TARGET_NAME} STATIC
    TestProgramData.cpp
    TestProgramData.h
    TestProgramEditorWidget.cpp
    TestProgramEditorWidget.h
    CommandTypeDelegate.cpp
    CommandTypeDelegate.h
    StepDetailPanel.cpp
    StepDetailPanel.h
    StepValidation.cpp
    StepValidation.h
    SignalSelectionInterface.h
    StepTableWidget.cpp
    StepTableWidget.h
    sub_step_table_widget.cpp
    sub_step_table_widget.h
    control_flow_config.h
    VerticalTabListDelegate.cpp
    VerticalTabListDelegate.h
    ExecutionControlBar.cpp
    ExecutionControlBar.h
)

target_include_directories(${TARGET_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

set_target_properties(${TARGET_NAME} PROPERTIES AUTOMOC ON)
target_link_libraries(${TARGET_NAME} PUBLIC Qt5::Widgets Qt5::Core Qt5::Gui etest_api etest_core etest_core_ui )