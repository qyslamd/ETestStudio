add_etest(
    NAME test_lua_cpp_interop
    SOURCES test_lua_cpp_interop.cpp
    LIBS liblua
    INCLUDES ${CMAKE_SOURCE_DIR}/3rdparty/lua-5.4.4/src
    LABELS LUA
    NO_RUN_SCRIPT
)
