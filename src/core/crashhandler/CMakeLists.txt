set(SOURCES
    CrashHandler.cpp
)

set(HEADERS
    CrashHandler.h
)

if(WIN32)
    list(APPEND SOURCES WindowsCrashHandler.cpp)
    list(APPEND HEADERS WindowsCrashHandler.h)
endif()

add_library(core_crashhandler STATIC ${SOURCES} ${HEADERS})
target_link_libraries(core_crashhandler PRIVATE Qt5::Core Qt5::Widgets)

if(WIN32)
    target_link_libraries(core_crashhandler PRIVATE dbghelp)
endif()

target_include_directories(core_crashhandler PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
