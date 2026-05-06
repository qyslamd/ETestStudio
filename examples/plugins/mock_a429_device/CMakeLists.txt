set(PLUGIN_NAME mock_a429_device)

add_library(${PLUGIN_NAME} SHARED
    MockA429Plugin.cpp
    MockA429Plugin.h
    mock_a429_device.json
)

target_include_directories(${PLUGIN_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/core/logger
    ${CMAKE_SOURCE_DIR}/src/core/config
    ${CMAKE_SOURCE_DIR}/src/core/plugin
)

target_link_libraries(${PLUGIN_NAME} PRIVATE
    core
)

set_target_properties(${PLUGIN_NAME} PROPERTIES
    AUTOMOC ON
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins"
)

if(WIN32)
    set_target_properties(${PLUGIN_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/plugins"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/plugins"
    )
endif()
