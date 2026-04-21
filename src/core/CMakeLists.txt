# core 核心库CMake配置
add_subdirectory(crashhandler)
add_subdirectory(config)

add_library(core STATIC
    logger/Logger.cpp
)

target_include_directories(core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(core PUBLIC
    Qt5::Core
    spdlog::spdlog
    core_crashhandler
    core_config
)

# 编译选项
if(MSVC)
    target_compile_options(core PRIVATE /W4 /WX-)
else()
    target_compile_options(core PRIVATE -Wall -Wextra -pedantic)
endif()
