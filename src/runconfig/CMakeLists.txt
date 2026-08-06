#=======================================
# 运行编辑器模块（etest_runconfig）
# 运行配置编辑器：RunConfig 数据模型（.erun）+ RunConfigEditor 主体
# + 3 个私有控件（测试程序清单/调色板/属性面板）。命名空间 etest::runconfig。
# 依赖 etest_visualizer（VisualizationArea/VisualizerProxy）、etest_ui（DockTitleBar）、
# etest_core（ProjectManager/Logger）、etest_core_ui（AppIconProvider）、etest_api（IEditor）。
# 运行态消费链（ExecutionPanelController）留在 app，仅消费 RunConfig 模型。
#=======================================

set(TARGET_NAME "etest_runconfig")

add_library(${TARGET_NAME} STATIC
    RunConfig.h
    RunConfig.cpp
    RunConfigEditor.h
    RunConfigEditor.cpp
    ProgramChecklistWidget.h
    ProgramChecklistWidget.cpp
    VisualizerPaletteWidget.h
    VisualizerPaletteWidget.cpp
    MonitorPropertyWidget.h
    MonitorPropertyWidget.cpp
)

set_target_properties(${TARGET_NAME} PROPERTIES
    AUTOMOC ON
)

# PUBLIC 传递 include 路径：消费方以 src 为根引用 runconfig/RunConfig.h
target_include_directories(${TARGET_NAME}
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(${TARGET_NAME}
    PUBLIC
    Qt5::Widgets
    Qt5::Core
    Qt5::Gui
    etest_api
    etest_core
    etest_core_ui
    etest_visualizer
    etest_ui
)
