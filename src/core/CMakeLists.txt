set(TARGET_NAME core)

set(SOURCES
    logger/Logger.cpp
    config/ConfigManager.cpp
    crashhandler/CrashHandler.cpp
    common/FileException.cpp
    common/TimeException.cpp
    common/StringException.cpp
    common/ByteException.cpp
    utils/FileUtil.cpp
    utils/TimeUtil.cpp
    utils/StringUtil.cpp
    utils/ByteUtil.cpp
)

set(HEADERS
    logger/Logger.h
    config/ConfigManager.h
    config/ConfigDefs.h
    crashhandler/CrashHandler.h
    common/FileException.h
    common/TimeException.h
    common/StringException.h
    common/ByteException.h
    utils/FileUtil.h
    utils/TimeUtil.h
    utils/StringUtil.h
    utils/ByteUtil.h
)

if(WIN32)
    list(APPEND SOURCES crashhandler/WindowsCrashHandler.cpp)
    list(APPEND HEADERS crashhandler/WindowsCrashHandler.h)
endif()

# ============================
# 添加静态库目标
add_library(${TARGET_NAME} STATIC ${SOURCES} ${HEADERS})

set_target_properties(${TARGET_NAME} PROPERTIES
    AUTOUIC ON
    AUTOMOC ON
    AUTORCC ON
)
target_include_directories(${TARGET_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/logger
    ${CMAKE_CURRENT_SOURCE_DIR}/config
    ${CMAKE_CURRENT_SOURCE_DIR}/crashhandler
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
)

target_link_libraries(${TARGET_NAME} PUBLIC
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    spdlog::spdlog
)

if(WIN32)
    target_link_libraries(${TARGET_NAME} PRIVATE dbghelp)
endif()

# 编译选项
if(MSVC)
    target_compile_options(${TARGET_NAME} PRIVATE /W4 /WX-)
else()
    target_compile_options(${TARGET_NAME} PRIVATE -Wall -Wextra -pedantic)
endif()
