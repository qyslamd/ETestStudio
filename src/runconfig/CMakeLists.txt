#=======================================
# 运行编辑器模块（etest_runconfig）
# 运行配置编辑器：RunConfig 数据模型（.erun）+ RunConfigEditor 主体
# + 3 个私有控件（测试程序清单/可视化组件面板/属性面板）+ 可视化组件
# （VisualizationArea 画布 + visualizers/ 信号可视化）。命名空间 etest::runconfig。
# 依赖 etest_visualizer 已并入本模块；etest_ui（DockTitleBar）、
# etest_core（ProjectManager/Logger）、etest_core_ui（AppIconProvider）、
# etest_api（IEditor）、etest_engine（MonitorSample，visualizers 采样数据源）。
# 运行态消费链（ExecutionPanelController）留在 app，仅消费 RunConfig/可视化模型。
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
    VisualizationArea.h
    VisualizationArea.cpp
    visualizers/SignalVisualizer.h
    visualizers/SignalVisualizer.cpp
    visualizers/ValueLabelWidget.h
    visualizers/ValueLabelWidget.cpp
    visualizers/WaveformWidget.h
    visualizers/WaveformWidget.cpp
    visualizers/DigitalMeterWidget.h
    visualizers/DigitalMeterWidget.cpp
    visualizers/GaugeVisualizer.h
    visualizers/GaugeVisualizer.cpp
    visualizers/LedIndicator.h
    visualizers/LedIndicator.cpp
    visualizers/VisualizerFactory.h
    visualizers/VisualizerFactory.cpp
    visualizers/VisualizerProxy.h
    visualizers/VisualizerProxy.cpp
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
    etest_engine
    etest_ui
    qcustomplot
)
