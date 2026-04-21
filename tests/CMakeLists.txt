# Lua smoke tests: verify lua.exe and luaw.exe basic functionality
if(WIN32)
    add_test(NAME LuaSmokeBasic
        COMMAND ${CMAKE_COMMAND} -E chdir "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}" lua.exe -e "print('Lua smoke ok')")
    add_test(NAME LuaSmokeNoConsole
        COMMAND ${CMAKE_COMMAND} -E chdir "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}" luaw.exe -e "print('Lua smoke no-console')")
    add_test(NAME LuaSmokeBytecode
        COMMAND cmd /C "${CMAKE_SOURCE_DIR}/lua_smoke_bytecode.bat")
endif()

add_subdirectory(lua/unit_test)
#add_subdirectory(libharu)
#add_subdirectory(qscintilla)
#add_subdirectory(qxlsx)
#add_subdirectory(spdlog)