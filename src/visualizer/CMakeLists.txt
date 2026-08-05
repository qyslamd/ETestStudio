#=======================================
# 可视化区共享模块（etest_visualizer）
# 可视化区（VisualizationArea + VisualizerProxy + visualizers）下沉为独立共享层，
# 供主程序运行态（ExecutionDashboard）与独立运行程序（test-executor）共同消费。
# 纯 UI 展示层，命名空间 etest::visualizer；依赖 etest_engine（MonitorSample）、
# etest_core（logger）、etest_core_ui（ThemeManager）。
#=======================================

add_library(etest_visualizer STATIC
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

set_target_properties(etest_visualizer PROPERTIES
    AUTOMOC ON
)

# PUBLIC 传递 include 路径：消费方以 src 为根引用 execution/VisualizationArea.h
target_include_directories(etest_visualizer
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(etest_visualizer
    PUBLIC
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    qcustomplot
    etest_engine
    etest_core
    etest_core_ui
)
