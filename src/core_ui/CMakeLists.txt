set(TARGET_NAME "etest_core_ui")

add_library(${TARGET_NAME} STATIC
    AppIconProvider.cpp
    ThemeManager.cpp
    PixmapOpacity.cpp
)

target_include_directories(${TARGET_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${TARGET_NAME} PUBLIC
    etest_core
    Qt5::Gui
    Qt5::Widgets
)

set_target_properties(${TARGET_NAME} PROPERTIES
    AUTOMOC ON
)
