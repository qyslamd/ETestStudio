add_etest(
    NAME test_sol2_basic
    SOURCES test_sol2_basic.cpp
    LIBS sol2::sol2
    LABELS THIRD_PARTY
)

add_etest(
    NAME test_sol2_qt_binding
    SOURCES test_sol2_qt_binding.cpp
    LIBS sol2::sol2 Qt5::Core
    LABELS THIRD_PARTY
)

add_etest(
    NAME test_lua_debugger
    SOURCES test_lua_debugger.cpp
    LIBS sol2::sol2 Qt5::Core
    LABELS THIRD_PARTY
)

add_etest(
    NAME test_lua_vm_isolation
    SOURCES test_lua_vm_isolation.cpp
    LIBS sol2::sol2 Qt5::Core
    LABELS THIRD_PARTY
)

add_etest(
    NAME test_lua_coroutine
    SOURCES test_lua_coroutine.cpp
    LIBS sol2::sol2 Qt5::Core
    LABELS THIRD_PARTY
)