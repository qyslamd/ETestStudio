#=======================================
# 版本管理
include(${CMAKE_SOURCE_DIR}/cmake/version_manage.cmake)
manage_version(
    VERSION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/version.txt"
    OUTPUT_PREFIX "APP"
)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/version.h
    @ONLY)

#=======================================
# 添加可执行文件
set(TARGET_NAME ${PROJECT_NAME})
add_executable(${TARGET_NAME}
    main.cpp
    MainWindow.h
    MainWindow.cpp
    ActivityBarWidget.h
    ActivityBarWidget.cpp
    SidebarWidget.h
    SidebarWidget.cpp
    FileExplorerWidget.h
    FileExplorerWidget.cpp
    FileTypeIconProvider.h
    FileTypeIconProvider.cpp
    GitWidget.h
    GitWidget.cpp
    HardwareTreeWidget.h
    HardwareTreeWidget.cpp
    EditorWidget.h
    EditorWidget.cpp
    EditorManager.h
    EditorManager.cpp
    OutputPanel.h
    OutputPanel.cpp
    PanelContainerWidget.h
    PanelContainerWidget.cpp
    ProblemsPanel.h
    ProblemsPanel.cpp
    TerminalPanel.h
    TerminalPanel.cpp
    SettingsWidget.h
    SettingsWidget.cpp
    SearchWidget.h
    SearchWidget.cpp
    WelcomeWidget.h
    WelcomeWidget.cpp
    dialogs/AnimationDialog.h
    dialogs/AnimationDialog.cpp
    dialogs/NewProjectDialog.h
    dialogs/NewProjectDialog.cpp
    utils/window_mover.h
    utils/window_mover.cpp
    utils/eph_qt_switch_button.h
    utils/eph_qt_switch_button.cpp
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
# 添加第三方依赖头文件路径
target_include_directories(${TARGET_NAME}
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/3rdparty/libharu-2.4.6/include
    ${CMAKE_BINARY_DIR}/3rdparty/libharu-2.4.6/include
    ${CMAKE_SOURCE_DIR}/3rdparty/Qt-Advanced-Docking-System-3.8.3/src
)

#=======================================
# 生成 Windows .rc 文件（版本信息嵌入 exe）
if(WIN32 AND MSVC)
    set(OUT_RC_FILE ${CMAKE_CURRENT_BINARY_DIR}/app.rc)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/app.rc.in
        ${OUT_RC_FILE}
        @ONLY)
    target_sources(${TARGET_NAME} PRIVATE ${OUT_RC_FILE})
endif()

#=======================================
# 链接Qt库
target_link_libraries(${TARGET_NAME}
    PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    core
    # 已配置第三方依赖
    spdlog::spdlog
    gtest
    gmock
    hpdf
    qtadvanceddocking
    QXlsx::QXlsx
    qsci
    )

#=======================================
# 生成运行脚本，自动设置Qt运行时环境变量
if(WIN32)
    # QT_BIN_DIR 和 QT_PLUGINS_DIR 在顶层CMakeLists.txt中设置
    configure_file(
        "${CMAKE_SOURCE_DIR}/cmake/run_app.bat.in"
        "${CMAKE_SOURCE_DIR}/run_app.bat"
        @ONLY
    )
endif()