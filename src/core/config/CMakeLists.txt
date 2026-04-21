set(CMAKE_AUTOMOC ON)
set(SOURCES
    ConfigManager.cpp
)

set(HEADERS
    ConfigManager.h
    ConfigDefs.h
)

add_library(core_config STATIC ${SOURCES} ${HEADERS})
target_link_libraries(core_config PRIVATE Qt5::Core)
target_include_directories(core_config PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
