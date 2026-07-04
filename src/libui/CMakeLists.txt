set(TARGET_NAME etest_ui)

add_library(${TARGET_NAME} STATIC
    dock_title_bar/DockTitleBar.h
    dock_title_bar/DockTitleBar.cpp
    tab_bar/TabBarStyle.h
    tab_bar/TabBarStyle.cpp
)

target_include_directories(${TARGET_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(${TARGET_NAME} PUBLIC
    Qt5::Widgets Qt5::Core Qt5::Gui etest_core
)

set_target_properties(${TARGET_NAME} PROPERTIES AUTOMOC ON)
