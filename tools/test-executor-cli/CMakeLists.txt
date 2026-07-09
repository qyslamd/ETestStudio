set(TARGET_NAME "test-executor-cli")

if(NOT TARGET etest_engine)
    add_subdirectory(${CMAKE_SOURCE_DIR}/src/engine ${CMAKE_BINARY_DIR}/engine_build)
endif()
if(NOT TARGET etest_core)
    add_subdirectory(${CMAKE_SOURCE_DIR}/src/core ${CMAKE_BINARY_DIR}/core_build)
endif()

add_executable(${TARGET_NAME} main.cpp)

target_link_libraries(${TARGET_NAME} PRIVATE
    etest_engine
    etest_core
    Qt5::Core
    spdlog::spdlog
)

target_include_directories(${TARGET_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

set_target_properties(${TARGET_NAME} PROPERTIES
    AUTOMOC ON
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
