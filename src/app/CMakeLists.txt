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
    MainWindow.h
    MainWindow.cpp
    widgets/HintButton.h
    widgets/HintButton.cpp
    widgets/HintPopup.h
    widgets/HintPopup.cpp
    widgets/HintMessageDelegate.h
    widgets/HintMessageDelegate.cpp
    widgets/MessageService.h
    widgets/MessageService.cpp
    ActivityBarWidget.h
    ActivityBarWidget.cpp
    AppStatusBarController.h
    AppStatusBarController.cpp
    TuxSaverController.h
    TuxSaverController.cpp
    EditorPanelController.h
    EditorPanelController.cpp
    ProjectController.h
    ProjectController.cpp
    ExecutionDashboard.h
    ExecutionDashboard.cpp
    ExecutionPanelController.h
    ExecutionPanelController.cpp
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
    TestProgramManagerWidget.h
    TestProgramManagerWidget.cpp
    ExecutionDebugWidget.h
    ExecutionDebugWidget.cpp
    editors/TextEditorWidget.h
    editors/TextEditorWidget.cpp
    editors/ImageViewerWidget.h
    editors/ImageViewerWidget.cpp
    editors/EtlogViewerWidget.h
    editors/EtlogViewerWidget.cpp
    editors/MockConfigEditor.h
    editors/MockConfigEditor.cpp
    EditorManager.h
    EditorManager.cpp
    editors/EditorFactory.h
    editors/EditorFactory.cpp
    widgets/LogFilter.h
    widgets/LogFilter.cpp
    widgets/LogFilterBar.h
    widgets/LogFilterBar.cpp
    widgets/LogOutputPanel.h
    widgets/LogOutputPanel.cpp
    widgets/ExecutionOutputPanel.h
    widgets/ExecutionOutputPanel.cpp
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
    welcome/WelcomeWidget.h
    welcome/WelcomeWidget.cpp
    welcome/v1/WelcomeV1Widget.h
    welcome/v1/WelcomeV1Widget.cpp
    welcome/v2/WelcomeV2Widget.h
    welcome/v2/WelcomeV2Widget.cpp
    welcome/v1/grid/grid_layout.h
    welcome/v1/grid/grid_layout.cpp
    welcome/v1/grid/grid_tile.h
    welcome/v1/grid/grid_tile.cpp
    welcome/v1/grid/gradientpainter.h
    welcome/v1/grid/gradientpainter.cpp
    welcome/v1/grid/layout_calculator_base.h
    welcome/v1/grid/layout_calculator_base.cpp
    welcome/v1/grid/layout_calculator_v1.h
    welcome/v1/grid/layout_calculator_v1.cpp
    welcome/v1/grid/layout_calculator_v3.h
    welcome/v1/grid/layout_calculator_v3.cpp
    welcome/v1/grid/grid_animator.h
    welcome/v1/grid/grid_animator.cpp
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
    widgets/StartupSplashWidget.h
    widgets/StartupSplashWidget.cpp
    widgets/RecentProjectCard.h
    widgets/RecentProjectCard.cpp
    widgets/RecentProjOrFileDelegate.h
    widgets/RecentProjOrFileDelegate.cpp
    widgets/WelcomeRecentDelegate.h
    widgets/WelcomeRecentDelegate.cpp
    widgets/ProjectTreeDelegate.h
    widgets/ProjectTreeDelegate.cpp
    dialogs/OverlayDialog.h
    dialogs/OverlayDialog.cpp
    wizards/BaseWizardDialog.h
    wizards/BaseWizardDialog.cpp
    wizards/NewProjectWizard.h
    wizards/NewProjectWizard.cpp
    wizards/WizardTemplateCard.h
    wizards/WizardTemplateCard.cpp
    dialogs/StepEditDialog.h
    dialogs/StepEditDialog.cpp
    wizards/TestProgramWizard.h
    wizards/TestProgramWizard.cpp
    wizards/ProtocolFileWizard.h
    wizards/ProtocolFileWizard.cpp
    wizards/TopologyFileWizard.h
    wizards/TopologyFileWizard.cpp
    wizards/NewFileGuideWizard.h
    wizards/NewFileGuideWizard.cpp
    wizards/QuickStartWizard.h
    wizards/QuickStartWizard.cpp
    dialogs/AboutDialog.h
    dialogs/AboutDialog.cpp
    dialogs/LoginDialog.h
    dialogs/LoginDialog.cpp
    dialogs/UserManagerDialog.h
    dialogs/UserManagerDialog.cpp
    utils/window_mover.h
    utils/window_mover.cpp
    utils/switch_button.h
    utils/switch_button.cpp
    utils/WizardStepBar.h
    utils/WizardStepBar.cpp
    utils/SignalSyncHelper.h
    utils/SignalSyncHelper.cpp
    dialogs/SignalSelectionDialog.h
    dialogs/SignalSelectionDialog.cpp
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
    set(APP_ICON_PATH "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/app_icon.ico")
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
    etest_core_ui
    etest_api
    etest_topology
    etest_protocol
    etest_program
    etest_ui
    icd_utility
    etest_engine
    etest_runconfig
    # 已配置第三方依赖
    spdlog::spdlog
    hpdf
    qtadvanceddocking
    QXlsx::QXlsx
    qsci
    qcustomplot
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
configure_run_script("run_app")

#=======================================
# 拷贝测试工程到输出目录，方便调试
# temp/projects 下可能包含多个项目目录，复制后保持为 test_projects/<project_name>/...
set(TEST_PROJECTS_SOURCE_DIR "${CMAKE_SOURCE_DIR}/temp/projects")
set(TEST_PROJECTS_OUTPUT_DIR "$<TARGET_FILE_DIR:${TARGET_NAME}>/test_projects")
if(EXISTS "${TEST_PROJECTS_SOURCE_DIR}")
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E remove_directory
            "${TEST_PROJECTS_OUTPUT_DIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${TEST_PROJECTS_OUTPUT_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
            "${TEST_PROJECTS_SOURCE_DIR}"
            "${TEST_PROJECTS_OUTPUT_DIR}"
        COMMENT "Copying test projects to output directory..."
    )
endif()

#=======================================
# 安装打包工具链（windeployqt / ISCC 均为 Windows 专属工具）
#=======================================
if(WIN32)

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

endif() # if(WIN32)

#=======================================
# 架构变量（供 configure_file 注入到 .iss.in / .bat.in）
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(APP_ARCH "x64")
    set(ISCC_APP_ID "{{6811a736-4ffd-4dae-a7a7-459a6e3d572f}}")
    set(ISCC_APP_PF "{autopf64}")
    set(ISCC_VC_REDIST "vc_redist.x64.exe")
    set(ISCC_APP_ARCH_SUFFIX "x64")
    set(ISCC_USE_64BIT "YES")
else()
    set(APP_ARCH "x86")
    set(ISCC_APP_ID "{{46AB6095-36F9-49F4-ABB1-6A36D8BB7E39}}")
    set(ISCC_APP_PF "{autopf}")
    set(ISCC_VC_REDIST "vc_redist.x86.exe")
    set(ISCC_APP_ARCH_SUFFIX "x86")
    set(ISCC_USE_64BIT "NO")
endif()

set(PACKAGE_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/dist")
set(ISCC_WHERE_ARE_FILES "${CMAKE_BINARY_DIR}/bin")

# 生成打包脚本（可独立编译的 .iss + CLI .bat）
configure_file(
    "${CMAKE_SOURCE_DIR}/scripts/make_install_package.iss.in"
    "${CMAKE_BINARY_DIR}/scripts/make_install_package.iss"
    @ONLY
)
configure_file(
    "${CMAKE_SOURCE_DIR}/scripts/make_installer.bat.in"
    "${CMAKE_BINARY_DIR}/scripts/make_installer.bat"
    @ONLY
)

# make_package target — 调用生成的 .bat 完成 windeployqt + ISCC
if(WIN32 AND EXISTS ${ISCC_EXECUTABLE})
    add_custom_target(make_package
        COMMAND "${CMAKE_BINARY_DIR}/scripts/make_installer.bat"
            "$<TARGET_FILE_DIR:${TARGET_NAME}>"
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:${TARGET_NAME}>"
        COMMENT "Building installer package with Inno Setup (${APP_ARCH})..."
    )
    add_dependencies(make_package ${TARGET_NAME})
endif()