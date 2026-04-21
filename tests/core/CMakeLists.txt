if(WIN32)
    add_executable(test_crashhandler_basic crashhandler_test.cpp)
    target_link_libraries(test_crashhandler_basic PRIVATE
        core
        gtest
        gtest_main
        Qt5::Core
    )
    
    add_test(NAME CrashHandlerBasicTest
        COMMAND test_crashhandler_basic
        WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    )
endif()
