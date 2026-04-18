#=======================================
# 添加可执行文件
set(TARGET_NAME ${PROJECT_NAME})
add_executable(${TARGET_NAME}
    main.cpp
    resource.qrc
)

#=======================================
# cmake 自动Qt MOC UIC RCC
set_target_properties(${TARGET_NAME} PROPERTIES 
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)

#=======================================
# 链接Qt库
target_link_libraries(${TARGET_NAME}
    PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    # 第三方依赖
    spdlog::spdlog
    )

#=======================================
# 生成运行脚本
set(QT_ROOT_DIR "${Qt5_DIR}/../../..")

if(WIN32)
    set(QT_BIN_DIR "${QT_ROOT_DIR}/bin")
    set(QT_PLUGINS_DIR "${QT_ROOT_DIR}/plugins")

    if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        set(BUILD_TYPE "ninja-debug")
    else()
        set(BUILD_TYPE "ninja-release")
    endif()

    configure_file(
        "${CMAKE_SOURCE_DIR}/run_app.bat.in"
        "${CMAKE_SOURCE_DIR}/run_app.bat"
        @ONLY
    )
endif()