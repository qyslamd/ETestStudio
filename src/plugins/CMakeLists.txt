option(BUILD_MOCK_PLUGINS "Build mock device plugins" ON)

if(BUILD_MOCK_PLUGINS)
    add_subdirectory(mock/serial)
    add_subdirectory(mock/can)
    add_subdirectory(mock/a429)
    add_subdirectory(mock/ad)
    add_subdirectory(mock/da)

    if(TARGET make_package)
        add_dependencies(make_package
            mock_serial_device
            mock_can_device
            mock_a429_device
            mock_ad_device
            mock_da_device
        )
    endif()
endif()
