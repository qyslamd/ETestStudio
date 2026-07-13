option(BUILD_MOCK_PLUGINS "Build mock device plugins" ON)

if(BUILD_MOCK_PLUGINS)
    add_subdirectory(mock/serial)
    add_subdirectory(mock/can)
    add_subdirectory(mock/a429)
    add_subdirectory(mock/ad)
    add_subdirectory(mock/da)
endif()
