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
add_executable(${TARGET_NAME} WIN32
    main.cpp
    main_window.h
    main_window.cpp
    widgets/HintBarWidget.h
    widgets/HintBarWidget.cpp
    ActivityBarWidget.h
    ActivityBarWidget.cpp
    SidebarWidget.h
    SidebarWidget.cpp
    ProjectStructureWidget.h
    ProjectStructureWidget.cpp
    GitWidget.h
    GitWidget.cpp
    HardwareTreeWidget.h
    HardwareTreeWidget.cpp
    ProtocolManagerWidget.h
    ProtocolManagerWidget.cpp
    TopologyManagerWidget.h
    TopologyManagerWidget.cpp
    TestProgramData.h
    TestProgramData.cpp
    TestProgramManagerWidget.h
    TestProgramManagerWidget.cpp
    editors/TestProgramEditorWidget.h
    editors/TestProgramEditorWidget.cpp
    editors/TextEditorWidget.h
    editors/TextEditorWidget.cpp
    editors/ImageViewerWidget.h
    editors/ImageViewerWidget.cpp
    EditorManager.h
    EditorManager.cpp
    editors/EditorFactory.h
    editors/EditorFactory.cpp
    widgets/OutputPanel.h
    widgets/OutputPanel.cpp
    widgets/BottomContainerWidget.h
    widgets/BottomContainerWidget.cpp
    widgets/ProblemsPanel.h
    widgets/ProblemsPanel.cpp
    TerminalPanel.h
    TerminalPanel.cpp
    dialogs/SettingsDialog.h
    dialogs/SettingsDialog.cpp
    SearchWidget.h
    SearchWidget.cpp
    WelcomeWidget.h
    WelcomeWidget.cpp
    grid/grid_layout.h
    grid/grid_layout.cpp
    grid/grid_tile.h
    grid/grid_tile.cpp
    grid/gradientpainter.h
    grid/gradientpainter.cpp
    grid/layout_calculator_base.h
    grid/layout_calculator_base.cpp
    grid/layout_calculator_v1.h
    grid/layout_calculator_v1.cpp
    grid/layout_calculator_v3.h
    grid/layout_calculator_v3.cpp
    grid/grid_animator.h
    grid/grid_animator.cpp
    widgets/clock_renderer.h
    widgets/modern_clock_renderer.h
    widgets/modern_clock_renderer.cpp
    widgets/minimal_clock_renderer.h
    widgets/minimal_clock_renderer.cpp
    widgets/PaintedClockWidget.h
    widgets/PaintedClockWidget.cpp
    widgets/EyeWidget.h
    widgets/EyeWidget.cpp
    widgets/TuxSaverOverlay.h
    widgets/TuxSaverOverlay.cpp
    widgets/SaverWidgetBase.h
    widgets/TuxSaverWidget.h
    widgets/TuxSaverWidget.cpp
    widgets/WisdomWidget.h
    widgets/WisdomWidget.cpp
    widgets/WisdomDatabase.h
    widgets/WisdomDatabase.cpp
    widgets/LoadingOverlay.h
    widgets/LoadingOverlay.cpp
    widgets/RecentProjectCard.h
    widgets/RecentProjectCard.cpp
    widgets/OpenFileDelegate.h
    widgets/OpenFileDelegate.cpp
    dialogs/AnimationDialog.h
    dialogs/AnimationDialog.cpp
    dialogs/NewProjectDialog.h
    dialogs/NewProjectDialog.cpp
    dialogs/AboutDialog.h
    dialogs/AboutDialog.cpp
    dialogs/LoginDialog.h
    dialogs/LoginDialog.cpp
    dialogs/UserManagerDialog.h
    dialogs/UserManagerDialog.cpp
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
    ${CMAKE_SOURCE_DIR}/src
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
    Qt5::Sql
    etest_core
    etest_api
    etest_topology
    etest_protocol
    etest_ui
    # 已配置第三方依赖
    spdlog::spdlog
    hpdf
    qtadvanceddocking
    QXlsx::QXlsx
    qsci
    SARibbonBar::SARibbonBar
    )


find_package(Qt5 COMPONENTS LinguistTools CONFIG)
if(Qt5LinguistTools_FOUND)
    message(STATUS "Qt5LinguistTools found, enabling translations")
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/resources/translations)
    set_source_files_properties(
        translations/ads_zh_CN.ts
        translations/SARibbon_zh_CN.ts
        PROPERTIES OUTPUT_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/resources/translations"
    )
    qt5_add_translation(QM_FILES
        translations/ads_zh_CN.ts
        translations/SARibbon_zh_CN.ts
    )
endif()

#=======================================
# 生成运行脚本，自动设置Qt运行时环境变量
if(WIN32)
    # QT_BIN_DIR 和 QT_PLUGINS_DIR 在顶层CMakeLists.txt中设置
    configure_file(
        "${CMAKE_SOURCE_DIR}/cmake/run_app.bat.in"
        "${CMAKE_BINARY_DIR}/bin/run_app.bat"
        @ONLY
    )
endif()

#=======================================
# 拷贝测试工程到输出目录，方便调试
if(EXISTS "${CMAKE_SOURCE_DIR}/temp/projects/Demo_Project3")
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
            "${CMAKE_SOURCE_DIR}/temp/projects/Demo_Project3"
            "$<TARGET_FILE_DIR:${TARGET_NAME}>/test_projects"
        COMMENT "Copying test projects to output directory..."
    )
endif()

#=======================================
# 安装打包工具链
#=======================================

# 查找 windeployqt — 从 Qt5::qmake 目标推导 bin 目录
get_target_property(_qt_qmake Qt5::qmake IMPORTED_LOCATION)
get_filename_component(QT_BIN_DIR "${_qt_qmake}" DIRECTORY)

find_program(WINDEPLOYQT_EXECUTABLE
    NAMES windeployqt
    HINTS "${QT_BIN_DIR}"
    NO_DEFAULT_PATH
)
if(NOT EXISTS ${WINDEPLOYQT_EXECUTABLE})
    message(WARNING "windeployqt.exe not found; Qt DLLs will not be auto-deployed")
endif()

# 查找 ISCC (Inno Setup Compiler)
find_program(ISCC_EXECUTABLE
    NAMES ISCC
    HINTS "${CMAKE_SOURCE_DIR}/3rdparty/Inno Setup 6"
)
if(NOT EXISTS ${ISCC_EXECUTABLE})
    message(WARNING "ISCC.exe not found; installer package target unavailable")
endif()

#=======================================
# make_deploy target — 按需触发 windeployqt
if(EXISTS ${WINDEPLOYQT_EXECUTABLE})
    add_custom_target(make_deploy
        COMMAND ${WINDEPLOYQT_EXECUTABLE}
            --verbose 0
            --no-angle
            --no-opengl-sw
            --no-system-d3d-compiler
            --compiler-runtime
            "$<TARGET_FILE:${TARGET_NAME}>"
        COMMENT "Running windeployqt..."
    )
    add_dependencies(make_deploy ${TARGET_NAME})
endif()

#=======================================
# make_package target — windeployqt + ISCC 打包
if(EXISTS ${ISCC_EXECUTABLE})
    set(PACKAGE_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/dist")
    add_custom_target(make_package
        COMMAND ${CMAKE_COMMAND} -E make_directory "${PACKAGE_OUTPUT_DIR}"
        COMMAND ${WINDEPLOYQT_EXECUTABLE}
            --verbose 0
            --no-angle
            --no-opengl-sw
            --no-system-d3d-compiler
            --compiler-runtime
            "$<TARGET_FILE:${TARGET_NAME}>"
        COMMAND ${ISCC_EXECUTABLE}
            "/Qp"
            "/DWhereAreFiles=$<TARGET_FILE_DIR:${TARGET_NAME}>"
            "/DWhereToOutput=${PACKAGE_OUTPUT_DIR}"
            "/DMyAppVersion=${APP_VERSION}"
            "/DMyAppExeName=${TARGET_NAME}.exe"
            "/DMySetupIcon=${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/app_icon.ico"
            "/DMyProductName=${PROJECT_NAME}"
            "${CMAKE_CURRENT_SOURCE_DIR}/make_install_package.iss"
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:${TARGET_NAME}>"
        COMMENT "Building installer package with Inno Setup..."
    )
    add_dependencies(make_package ${TARGET_NAME})
endif()