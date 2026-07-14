# src/tools/CMakeLists.txt
option(BUILD_ETEST_TOOLS "Build standalone product tools" ON)

if(BUILD_ETEST_TOOLS)
    add_subdirectory(topology-editor)
    add_subdirectory(protocol-editor)
    add_subdirectory(test-program-editor)
    add_subdirectory(test-executor)
    add_subdirectory(test-executor-cli)
endif()
