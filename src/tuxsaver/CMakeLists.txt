set(TARGET_NAME "etest_tuxsaver")

add_library(${TARGET_NAME} STATIC
    TuxSaverWidget.h
    TuxSaverWidget.cpp
)

target_include_directories(${TARGET_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

set_target_properties(${TARGET_NAME} PROPERTIES AUTOMOC ON)
target_link_libraries(${TARGET_NAME} PUBLIC Qt5::Widgets Qt5::Core Qt5::Gui)
