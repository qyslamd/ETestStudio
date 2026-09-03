set(TARGET_NAME etest_ui)

add_library(${TARGET_NAME} STATIC
    styles/TabStyleConstants.h
    styles/TabBarStyle.h
    styles/TabBarStyle.cpp
    styles/NoFocusRectStyle.h
    styles/NoFocusRectStyle.cpp
    styles/DockWidgetTabStyle.h
    styles/DockWidgetTabStyle.cpp
    styles/DockAreaTabBarStyle.h
    styles/DockAreaTabBarStyle.cpp
    styles/EtestComponentsFactory.h
    styles/EtestComponentsFactory.cpp
    EtDockWidget.h
    EtDockWidget.cpp
)

target_include_directories(${TARGET_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(${TARGET_NAME} PUBLIC
    Qt5::Widgets Qt5::Core Qt5::Gui etest_core etest_core_ui
    qtadvanceddocking
)

set_target_properties(${TARGET_NAME} PROPERTIES AUTOMOC ON)
