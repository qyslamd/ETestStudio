# 统一配置Qt测试运行环境变量
if(Qt5_FOUND)
    get_target_property(_qt_core_location Qt5::Core LOCATION)
    get_filename_component(QT_LIB_DIR "${_qt_core_location}" DIRECTORY)
    if(WIN32)
        get_filename_component(QT_BIN_DIR "${QT_LIB_DIR}/../bin" ABSOLUTE)
        set(TEST_ENV_QT
            "PATH=${QT_BIN_DIR};$ENV{PATH}"
            "QT_PLUGIN_PATH=${QT_BIN_DIR}/../plugins"
        )
    else()
        set(TEST_ENV_QT
            "LD_LIBRARY_PATH=${QT_LIB_DIR}:$ENV{LD_LIBRARY_PATH}"
            "QT_PLUGIN_PATH=${QT_LIB_DIR}/../plugins"
        )
    endif()

    # 辅助函数：批量给指定目录下所有测试设置环境变量
    function(set_test_env_for_dir dir)
        get_property(tests DIRECTORY "${dir}" PROPERTY TESTS)
        if(tests)
            set_tests_properties(${tests} PROPERTIES ENVIRONMENT "${TEST_ENV_QT}")
        endif()
    endfunction()
endif()

# Lua smoke tests: verify lua.exe and luaw.exe basic functionality
if(WIN32)
    add_test(NAME LuaSmokeBasic
        COMMAND ${CMAKE_COMMAND} -E chdir "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}" lua.exe -e "print('Lua smoke ok')")
    add_test(NAME LuaSmokeNoConsole
        COMMAND ${CMAKE_COMMAND} -E chdir "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}" luaw.exe -e "print('Lua smoke no-console')")
add_test(NAME LuaSmokeBytecode
    COMMAND ${CMAKE_COMMAND} -E chdir "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
    luac.exe -o test_bytecode.luac ${CMAKE_CURRENT_LIST_DIR}/lua/batch_smoke_test/test.lua
    lua.exe test_bytecode.luac)
endif()

add_subdirectory(lua/unit_test)
add_subdirectory(libharu)
add_subdirectory(qscintilla)
add_subdirectory(qxlsx)
add_subdirectory(spdlog)
add_subdirectory(core core_test)