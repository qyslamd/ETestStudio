# ============== Lua冒烟测试（修复硬编码兼容跨平台） ==============
if(WIN32)
    set(LUA_EXECUTABLE "lua.exe")
    set(LUAC_EXECUTABLE "luac.exe")
else()
    set(LUA_EXECUTABLE "lua")
    set(LUAC_EXECUTABLE "luac")
endif()

add_test(NAME LuaSmokeBasic
    COMMAND ${CMAKE_COMMAND} -E chdir "${ETEST_GLOBAL_RUNTIME_OUTPUT_DIR}" ${LUA_EXECUTABLE} -e "print('Lua smoke ok')")
set_tests_properties(LuaSmokeBasic PROPERTIES LABELS "LUA;SMOKE" )

if(WIN32)
    add_test(NAME LuaSmokeNoConsole
        COMMAND ${CMAKE_COMMAND} -E chdir "${ETEST_GLOBAL_RUNTIME_OUTPUT_DIR}" luaw.exe -e "print('Lua smoke no-console')")
    set_tests_properties(LuaSmokeNoConsole PROPERTIES LABELS "LUA;SMOKE" )
endif()

# add_test(NAME LuaSmokeBytecode
#     COMMAND ${CMAKE_COMMAND} -E chdir "${ETEST_GLOBAL_RUNTIME_OUTPUT_DIR}"
#     cmd /c "${LUAC_EXECUTABLE} -o test_bytecode.luac \"${CMAKE_SOURCE_DIR}/tests/lua/batch_smoke_test/test.lua\" && ${LUA_EXECUTABLE} test_bytecode.luac")
set(TEST_LUA_FILE ${CMAKE_SOURCE_DIR}/tests/lua/batch_smoke_test/test.lua)
set(TEST_LUAC_OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_bytecode.luac)
add_test(NAME LuaGenBytecode
    COMMAND ${CMAKE_COMMAND} -E chdir "${ETEST_GLOBAL_RUNTIME_OUTPUT_DIR}" 
    ${LUAC_EXECUTABLE} -o ${TEST_LUAC_OUTPUT} ${TEST_LUA_FILE}
    )
set_tests_properties(LuaGenBytecode PROPERTIES LABELS "LUA;SMOKE")

add_test(NAME LuaSmokeBytecode
    COMMAND ${CMAKE_COMMAND} -E chdir "${ETEST_GLOBAL_RUNTIME_OUTPUT_DIR}" 
    ${LUAC_EXECUTABLE} -o ${TEST_LUAC_OUTPUT} ${TEST_LUA_FILE}
    )
set_tests_properties(LuaSmokeBytecode PROPERTIES LABELS "LUA;SMOKE")