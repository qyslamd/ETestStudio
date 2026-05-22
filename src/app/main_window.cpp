#include "main_window.h"

#include <QClipboard>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QInputDialog>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QScrollBar>
#include <QShortcut>
#include <QStandardPaths>
#include <QStatusBar>
#include <QToolButton>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <DockAreaTitleBar.h>
#include <DockAreaWidget.h>
#include <DockSplitter.h>
#include <DockWidgetTab.h>
#include "EditorManager.h"
#include "FileExplorerWidget.h"
#include "GitWidget.h"
#include "HardwareTreeWidget.h"
#include "OutputPanel.h"
#include "ProtocolManagerWidget.h"
#include "PanelContainerWidget.h"
#include "ProblemsPanel.h"
#include "SearchWidget.h"
#include "SettingsWidget.h"
#include "SidebarWidget.h"
#include "TerminalPanel.h"
#include "TextEditorWidget.h"
#include "WelcomeWidget.h"
#include "backup/BackupManager.h"
#include "config/ConfigDefs.h"
#include "config/ConfigManager.h"
#include "dialogs/NewProjectDialog.h"
#include "api/IEditor.h"
#include "logger/Logger.h"
#include "logger/QtConsoleSink.h"
#include "plugin/PluginManager.h"
#include "project/ProjectManager.h"
#include "topology/TopologyDocument.h"
#include "topology/TopologyEditorWidget.h"

using namespace etest::core::config;
using namespace etest::core::project;
using namespace etest::core::logger;

namespace etest::app {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      dock_manager_(nullptr),
      sidebar_(nullptr),
      editor_manager_(nullptr),
      output_panel_(nullptr),
      problems_panel_(nullptr),
      terminal_panel_(nullptr) {
  initUi();
  initSignals();
  restoreSession();
  updateWindowTitle();

  // 加载插件并刷新硬件树
  auto& pluginMgr = etest::core::plugin::PluginManager::instance();
  pluginMgr.addSearchPath(QCoreApplication::applicationDirPath() + "/plugins");
  pluginMgr.loadAll();
  sidebar_->hardwareTree()->refreshTree();

  LOG_INFO("MAIN", "主窗口初始化完成");
}

MainWindow::~MainWindow() {
  // 清除ProjectManager中的脏检查回调，避免悬空指针
  auto& projectMgr = etest::core::project::ProjectManager::instance();
  projectMgr.setDirtyCheckCallback({});

  // 显式断开与ProjectManager的所有信号连接
  // Qt会在接收者销毁时自动断开，但显式断开更清晰
  projectMgr.disconnect(this);
}

void MainWindow::initUi() {
  setWindowTitle("ETest Demo");
  setMinimumSize(900, 600);
  setWindowIcon(QIcon(":/resources/icons/app_icon.ico"));

  // 加载VSCode风格样式表
  QFile styleFile(":/resources/styles/vscode.qss");
  if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
    setStyleSheet(QString::fromUtf8(styleFile.readAll()));
    styleFile.close();
  }

  createMenuBar();
  createEditMenu();
  createToolBar();
  createStatusBar();

  // QADS Dock Manager
  ads::CDockManager::setConfigFlag(ads::CDockManager::AlwaysShowTabs, true);
  dock_manager_ = new ads::CDockManager(this);

  // 覆盖QADS内置的default.css，应用暗色主题（必须设置到CDockManager自身才生效）
  QFile adsStyleFile(":/resources/styles/ads_dark.qss");
  if (adsStyleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
    dock_manager_->setStyleSheet(dock_manager_->styleSheet() +
                                 QString::fromUtf8(adsStyleFile.readAll()));
    adsStyleFile.close();
  }

  // 中央编辑区：Welcome页面（必须在添加其他dock之前建立）
  welcome_widget_ = new WelcomeWidget(this);
  auto* centralDock = new ads::CDockWidget(QStringLiteral("欢迎"));
  centralDock->setObjectName("CentralDock");
  centralDock->setWidget(welcome_widget_);
  centralDock->tabWidget()->setElideMode(Qt::ElideNone);
  dock_manager_->setCentralWidget(centralDock);

  // 隐藏中央区域标题栏的菜单和分离按钮，保留tab
  auto* centralArea = centralDock->dockAreaWidget();
  if (centralArea) {
    hideDockTitleBarButtons(centralArea);
  }

  // 编辑器管理器
  editor_manager_ = new EditorManager(dock_manager_, this);

  // 左侧：活动栏 + 侧边栏合并为一个 DockWidget
  sidebar_ = new SidebarWidget(this);
  sidebar_dock_ = new ads::CDockWidget(QStringLiteral("侧边栏"));
  sidebar_dock_->setObjectName("SidebarDock");
  sidebar_dock_->setWidget(sidebar_);
  sidebar_dock_->setFeature(ads::CDockWidget::DockWidgetClosable, false);
  sidebar_dock_->setFeature(ads::CDockWidget::DockWidgetFloatable, false);
  sidebar_dock_->setFeature(ads::CDockWidget::DockWidgetMovable, false);
  sidebar_dock_->tabWidget()->setElideMode(Qt::ElideNone);
  dock_manager_->addDockWidget(ads::LeftDockWidgetArea, sidebar_dock_);
  // 隐藏侧边栏标题栏
  sidebar_dock_->dockAreaWidget()->titleBar()->hide();

  // ==================== 底部：统一面板容器 ====================
  output_panel_ = new OutputPanel(this);
  problems_panel_ = new ProblemsPanel(this);
  terminal_panel_ = new TerminalPanel(this);

  // Terminal shell auto-starts via TerminalPanel::showEvent()

  panel_container_ = new PanelContainerWidget(this);
  panel_container_->addPanel(QStringLiteral("输出"), output_panel_);
  panel_container_->addPanel(QStringLiteral("问题"), problems_panel_);
  panel_container_->addPanel(QStringLiteral("终端"), terminal_panel_);

  auto* panelDock = new ads::CDockWidget(QStringLiteral("面板"));
  panelDock->setObjectName("PanelDock");
  panelDock->setWidget(panel_container_);
  panelDock->setFeature(ads::CDockWidget::DockWidgetClosable, true);
  panelDock->setFeature(ads::CDockWidget::DockWidgetFloatable, false);
  panelDock->setFeature(ads::CDockWidget::DockWidgetMovable, false);
  panelDock->tabWidget()->setElideMode(Qt::ElideNone);
  dock_manager_->addDockWidget(ads::BottomDockWidgetArea, panelDock);
  // 隐藏面板标题栏右侧的三个按钮（PanelContainerWidget内部已有tab和关闭按钮）
  hideDockTitleBarButtons(panelDock->dockAreaWidget());

  // 面板容器信号
  connect(panel_container_, &PanelContainerWidget::panelClosed, this,
          [panelDock]() { panelDock->closeDockWidget(); });
  connect(panel_container_, &PanelContainerWidget::panelMaximized, this,
          [this]() {
            if (sidebar_dock_)
              sidebar_dock_->closeDockWidget();
          });
  connect(panel_container_, &PanelContainerWidget::panelRestored, this,
          [this]() {
            if (sidebar_dock_) {
              sidebar_dock_->toggleView(true);
              if (sidebar_dock_->dockAreaWidget()) {
                sidebar_dock_->dockAreaWidget()->titleBar()->hide();
              }
            }
          });

  // ==================== 右侧：辅助侧边栏（默认隐藏） ====================
  auto* auxPlaceholder = new QLabel(QStringLiteral("辅助侧边栏"), this);
  auxPlaceholder->setAlignment(Qt::AlignCenter);
  auto* auxDock = new ads::CDockWidget(QStringLiteral("辅助侧边栏"));
  auxDock->setObjectName("AuxSidebarDock");
  auxDock->setWidget(auxPlaceholder);
  auxDock->setFeature(ads::CDockWidget::DockWidgetClosable, true);
  auxDock->setFeature(ads::CDockWidget::DockWidgetFloatable, false);
  auxDock->setFeature(ads::CDockWidget::DockWidgetMovable, false);
  auxDock->tabWidget()->setElideMode(Qt::ElideNone);
  dock_manager_->addDockWidget(ads::RightDockWidgetArea, auxDock);
  hideDockTitleBarButtons(auxDock->dockAreaWidget());
  // auxDock->closeDockWidget();  // 默认隐藏

  // 恢复窗口状态
  restoreWindowState();
}

void MainWindow::initSignals() {
  // View菜单显示时同步菜单项选中状态与实际dock状态
  if (view_menu_) {
    connect(view_menu_, &QMenu::aboutToShow, this, [this]() {
      auto* panelDock = dock_manager_->findDockWidget("PanelDock");
      if (panelDock && view_panel_action_) {
        view_panel_action_->setChecked(!panelDock->isClosed());
      }
      auto* auxDock = dock_manager_->findDockWidget("AuxSidebarDock");
      if (auxDock && view_aux_sidebar_action_) {
        view_aux_sidebar_action_->setChecked(!auxDock->isClosed());
      }
    });
  }


  // 项目管理信号
  auto& projectMgr = etest::core::project::ProjectManager::instance();
  connect(&projectMgr, &etest::core::project::ProjectManager::projectOpened,
          this, &MainWindow::onProjectOpened);
  connect(&projectMgr, &etest::core::project::ProjectManager::projectClosed,
          this, &MainWindow::onProjectClosed);
  connect(&projectMgr,
          &etest::core::project::ProjectManager::recentProjectsChanged, this,
          &MainWindow::updateRecentProjectsMenu);

  // 注册编辑器脏检查回调到ProjectManager
  projectMgr.setDirtyCheckCallback(
      [this]() { return editor_manager_->hasUnsavedChanges(); });

  // 文件浏览器：项目打开/关闭时设置根路径
  auto* fileExplorer = sidebar_->fileExplorer();
  connect(&projectMgr, &etest::core::project::ProjectManager::projectOpened,
          fileExplorer, [fileExplorer](const QString& projectPath) {
            fileExplorer->setRootPath(projectPath);
          });
  connect(&projectMgr, &etest::core::project::ProjectManager::projectClosed,
          fileExplorer, [fileExplorer]() { fileExplorer->setRootPath({}); });

  // 文件浏览器：双击文件打开编辑器
  connect(fileExplorer, &FileExplorerWidget::fileOpenRequested, editor_manager_,
          &EditorManager::openFile);
  // 文件浏览器：文件删除/重命名同步到编辑器
  connect(fileExplorer, &FileExplorerWidget::fileDeleted, editor_manager_,
          &EditorManager::onFileDeleted);
  connect(fileExplorer, &FileExplorerWidget::fileRenamed, editor_manager_,
          &EditorManager::onFileRenamed);

  // 搜索组件：项目打开/关闭时设置搜索根目录
  auto* searchWidget = sidebar_->searchWidget();
  connect(&projectMgr, &etest::core::project::ProjectManager::projectOpened,
          searchWidget, [searchWidget](const QString& projectPath) {
            searchWidget->setSearchRoot(projectPath);
          });
  connect(&projectMgr, &etest::core::project::ProjectManager::projectClosed,
          searchWidget, [searchWidget]() { searchWidget->setSearchRoot({}); });

  // 搜索组件：点击结果打开文件并跳转到行
  connect(searchWidget, &SearchWidget::fileOpenRequested, editor_manager_,
          &EditorManager::openFileAtLine);

  // Git面板：项目打开/关闭时设置根目录
  auto* gitWidget = sidebar_->gitWidget();
  connect(&projectMgr, &etest::core::project::ProjectManager::projectOpened,
          gitWidget, [gitWidget](const QString& projectPath) {
            gitWidget->setProjectRoot(projectPath);
          });
  connect(&projectMgr, &etest::core::project::ProjectManager::projectClosed,
          gitWidget, [gitWidget]() { gitWidget->setProjectRoot({}); });

  // 备份管理：项目打开/关闭时启停自动备份
  auto& backupMgr = etest::core::backup::BackupManager::instance();
  connect(&projectMgr, &etest::core::project::ProjectManager::projectOpened,
          &backupMgr, [&backupMgr](const QString& projectPath) {
            backupMgr.onProjectOpened(projectPath);
          });
  connect(&projectMgr, &etest::core::project::ProjectManager::projectClosed,
          &backupMgr, [&backupMgr]() { backupMgr.onProjectClosed(); });

  // 欢迎页：快捷操作和最近项目
  connect(welcome_widget_, &WelcomeWidget::newProjectRequested, this,
          &MainWindow::onNewProject);
  connect(welcome_widget_, &WelcomeWidget::openProjectRequested, this,
          &MainWindow::onOpenProject);
  connect(welcome_widget_, &WelcomeWidget::projectOpenRequested, this,
          &MainWindow::openRecentProject);

  // 项目打开后刷新欢迎页的最近项目列表
  connect(&projectMgr,
          &etest::core::project::ProjectManager::recentProjectsChanged,
          welcome_widget_, &WelcomeWidget::refreshRecentProjects);

  // Git面板：点击文件打开编辑器
  connect(gitWidget, &GitWidget::fileOpenRequested, editor_manager_,
          &EditorManager::openFile);

  // 编辑器：当前编辑器切换时更新状态栏和菜单状态
  connect(
      editor_manager_, &EditorManager::currentEditorChanged, this,
      [this](IEditor* editor) {
        bool hasEditor = (editor != nullptr);
        bool hasSelection = false;

        save_as_action_->setEnabled(hasEditor);
        close_file_action_->setEnabled(hasEditor);
        close_all_files_action_->setEnabled(hasEditor);

        bool isModified = hasEditor && editor->isModified();
        save_action_->setEnabled(isModified);

        // 断开之前编辑器的所有信号连接
        QObject::disconnect(current_editor_selection_connection_);
        QObject::disconnect(current_editor_state_connection_);

        if (hasEditor) {
          status_message_label_->setText(editor->filePath());

          if (auto* textEditor = dynamic_cast<TextEditorWidget*>(editor)) {
            QsciScintilla* sci_editor = textEditor->editor();
            hasSelection = sci_editor->hasSelectedText();

            int line, col;
            sci_editor->getCursorPosition(&line, &col);
            status_cursor_label_->setText(
                QStringLiteral("行 %1, 列 %2").arg(line + 1).arg(col + 1));
            status_language_label_->setText(QStringLiteral("纯文本"));
            status_eol_label_->setText(QStringLiteral("CRLF"));
            status_encoding_label_->setText(QStringLiteral("UTF-8"));

            current_editor_selection_connection_ =
                connect(sci_editor, &QsciScintilla::selectionChanged, this,
                        [this, sci_editor]() {
                          bool hasSelection = sci_editor->hasSelectedText();
                          edit_cut_action_->setEnabled(hasSelection);
                          edit_copy_action_->setEnabled(hasSelection);
                        });

            current_editor_state_connection_ =
                connect(textEditor, &TextEditorWidget::editorStateChanged, this,
                        [this, editor]() {
                          edit_undo_action_->setEnabled(editor->canUndo());
                          edit_redo_action_->setEnabled(editor->canRedo());
                        });

            edit_undo_action_->setEnabled(editor->canUndo());
            edit_redo_action_->setEnabled(editor->canRedo());
          } else if (auto* topoEditor =
                         dynamic_cast<etest::topology::TopologyEditorWidget*>(
                             editor)) {
            auto* stack = topoEditor->document()->undoStack();
            current_editor_state_connection_ = connect(
                stack, &QUndoStack::indexChanged, this, [this, editor]() {
                  edit_undo_action_->setEnabled(editor->canUndo());
                  edit_redo_action_->setEnabled(editor->canRedo());
                });
            edit_undo_action_->setEnabled(editor->canUndo());
            edit_redo_action_->setEnabled(editor->canRedo());
          } else {
            status_cursor_label_->setText(QStringLiteral("行 1, 列 1"));
            status_language_label_->setText(QStringLiteral("纯文本"));
            status_eol_label_->setText(QStringLiteral("CRLF"));
            status_encoding_label_->setText(QStringLiteral("UTF-8"));
            edit_undo_action_->setEnabled(false);
            edit_redo_action_->setEnabled(false);
          }
        } else {
          status_message_label_->setText(QStringLiteral("就绪"));
          status_cursor_label_->setText(QStringLiteral("行 1, 列 1"));
          status_language_label_->setText(QStringLiteral("纯文本"));
          status_eol_label_->setText(QStringLiteral("CRLF"));
          status_encoding_label_->setText(QStringLiteral("UTF-8"));
          edit_cut_action_->setEnabled(false);
          edit_copy_action_->setEnabled(false);
          edit_undo_action_->setEnabled(false);
          edit_redo_action_->setEnabled(false);
          edit_find_action_->setEnabled(false);
          edit_replace_action_->setEnabled(false);
          edit_go_to_line_action_->setEnabled(false);
        }
        updateWindowTitle();

        edit_cut_action_->setEnabled(hasSelection);
        edit_copy_action_->setEnabled(hasSelection);
        edit_paste_action_->setEnabled(hasEditor);
        edit_find_action_->setEnabled(hasEditor);
        edit_replace_action_->setEnabled(hasEditor);
        edit_go_to_line_action_->setEnabled(hasEditor);
      });

  // 编辑器：未保存更改状态变化时更新窗口标题和保存所有按钮
  connect(editor_manager_, &EditorManager::unsavedChangesChanged, this,
          [this]() {
            updateWindowTitle();
            // 有任何未保存的更改时启用保存所有按钮
            save_all_action_->setEnabled(editor_manager_->hasUnsavedChanges());
          });

  connect(editor_manager_, &EditorManager::modificationChanged, this,
          [this](bool modified) { save_action_->setEnabled(modified); });

  connect(
      editor_manager_, &EditorManager::fileOpened, this,
      [this](const QString&) { close_all_files_action_->setEnabled(true); });
  connect(editor_manager_, &EditorManager::fileClosed, this,
          [this](const QString&) {
            close_all_files_action_->setEnabled(
                editor_manager_->currentEditor() != nullptr);
          });

  // 手动初始化按钮状态，处理程序启动时已经有打开的编辑器的情况
  IEditor* current_editor = editor_manager_->currentEditor();
  bool hasEditor = (current_editor != nullptr);
  bool hasSelection = false;

  if (hasEditor) {
    if (auto* textEditor = dynamic_cast<TextEditorWidget*>(current_editor)) {
      QsciScintilla* sci_editor = textEditor->editor();
      hasSelection = sci_editor->hasSelectedText();
    }
  }

  bool isModified = hasEditor && current_editor->isModified();
  save_action_->setEnabled(isModified);

  edit_undo_action_->setEnabled(hasEditor);
  edit_redo_action_->setEnabled(hasEditor);
  edit_cut_action_->setEnabled(hasSelection);
  edit_copy_action_->setEnabled(hasSelection);
  edit_paste_action_->setEnabled(hasEditor);

  // 剪贴板处理：动态更新粘贴按钮状态
  clipboard_ = QGuiApplication::clipboard();
  auto updatePasteState = [this]() {
    bool hasEditor = (editor_manager_->currentEditor() != nullptr);
    bool hasText = !clipboard_->text().isEmpty();
    edit_paste_action_->setEnabled(hasEditor && hasText);
  };
  connect(clipboard_, &QClipboard::dataChanged, this, updatePasteState);
  updatePasteState();  // 初始化状态

  // VSCode风格快捷键
  // Ctrl+B 切换侧边栏
  auto* toggleSidebar = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_B), this);
  connect(toggleSidebar, &QShortcut::activated, this, [this]() {
    sidebar_->toggleContentPanel();
  });

  // Ctrl+Shift+F 全局搜索
  auto* globalSearchShortcut =
      new QShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_F), this);
  connect(globalSearchShortcut, &QShortcut::activated, this, [this]() {
    sidebar_->switchPage(1);
    sidebar_->setActiveIndex(1);
    if (sidebar_dock_ && sidebar_dock_->isClosed()) {
      sidebar_dock_->toggleView(true);
      if (sidebar_dock_->dockAreaWidget()) {
        sidebar_dock_->dockAreaWidget()->titleBar()->hide();
      }
    }
    if (auto* sw = sidebar_->searchWidget()) {
      sw->setFocusOnSearchInput();
    }
  });

  // 硬件树：插件加载/卸载时自动刷新
  auto* hardwareTree = sidebar_->hardwareTree();
  auto& pluginMgr = etest::core::plugin::PluginManager::instance();
  connect(&pluginMgr, &etest::core::plugin::PluginManager::pluginLoaded,
          hardwareTree, &HardwareTreeWidget::refreshTree);
  connect(&pluginMgr, &etest::core::plugin::PluginManager::pluginUnloaded,
          hardwareTree, &HardwareTreeWidget::refreshTree);

  // 协议管理器：双击文件打开编辑器
  auto* protocolMgr = sidebar_->protocolManager();
  connect(protocolMgr, &ProtocolManagerWidget::openFileRequested,
          editor_manager_, &EditorManager::openFile);

  // 协议管理器：项目打开/关闭时刷新
  connect(&projectMgr, &etest::core::project::ProjectManager::projectOpened,
          protocolMgr, &ProtocolManagerWidget::refreshList);
  connect(&projectMgr, &etest::core::project::ProjectManager::projectClosed,
          protocolMgr, &ProtocolManagerWidget::refreshList);

  // 日志输出到界面
  auto* qtSink = etest::core::logger::Logger::qtConsoleSink();
  if (qtSink) {
    connect(qtSink, &QtConsoleSink::logMessage, output_panel_,
            &OutputPanel::appendLog);
  }

  // 设置对话框
  connect(sidebar_, &SidebarWidget::settingsTriggered, this, [this]() {
    if (!settings_dialog_) {
      settings_dialog_ = new SettingsWidget(this);
    }
    settings_dialog_->show();
    settings_dialog_->raise();
    settings_dialog_->activateWindow();
  });

  // 内容面板显隐时调整 SidebarDock 宽度，让编辑器区域贴合
  connect(sidebar_, &SidebarWidget::contentPanelToggled, this, [this](bool visible) {
    auto* area = sidebar_dock_->dockAreaWidget();
    auto* splitter = ads::internal::findParent<ads::CDockSplitter*>(area);
    if (!visible) {
      sidebar_expanded_width_ = area->width();
      area->setFixedWidth(48);
      if (splitter) {
        auto sizes = splitter->sizes();
        int idx = splitter->indexOf(area);
        if (idx >= 0 && idx < sizes.size()) {
          sizes[idx] = 48;
          splitter->setSizes(sizes);
        }
      }
    } else {
      area->setMinimumWidth(0);
      area->setMaximumWidth(QWIDGETSIZE_MAX);
      if (splitter) {
        auto sizes = splitter->sizes();
        int idx = splitter->indexOf(area);
        if (idx >= 0 && idx < sizes.size()) {
          sizes[idx] = sidebar_expanded_width_;
          splitter->setSizes(sizes);
        }
      }
    }
  });
}

void MainWindow::createMenuBar() {
  auto* menuBar = this->menuBar();

  auto* fileMenu = menuBar->addMenu(QStringLiteral("文件(&F)"));
  fileMenu->setObjectName("fileMenu");

  new_project_action_ = fileMenu->addAction(QStringLiteral("新建项目"), this,
                                            &MainWindow::onNewProject);
  new_project_action_->setShortcut(QStringLiteral("Ctrl+Shift+N"));

  open_project_action_ = fileMenu->addAction(QStringLiteral("打开项目"), this,
                                             &MainWindow::onOpenProject);
  open_project_action_->setShortcut(QKeySequence::Open);
  close_project_action_ = fileMenu->addAction(QStringLiteral("关闭项目"), this,
                                              &MainWindow::onCloseProject);
  close_project_action_->setEnabled(false);

  fileMenu->addSeparator();

  save_action_ = fileMenu->addAction(QStringLiteral("保存"), this,
                                     &MainWindow::onSaveFile);
  save_action_->setShortcut(QKeySequence::Save);
  save_action_->setShortcutContext(
      Qt::ApplicationShortcut);  // 全局级快捷键，不受焦点控件影响
  save_action_->setEnabled(false);

  save_as_action_ = fileMenu->addAction(QStringLiteral("另存为..."), this,
                                        &MainWindow::onSaveFileAs);
  save_as_action_->setEnabled(false);

  save_all_action_ = fileMenu->addAction(QStringLiteral("保存所有"), this,
                                         &MainWindow::onSaveAllFiles);
  save_all_action_->setShortcut(QStringLiteral("Ctrl+Shift+S"));
  save_all_action_->setEnabled(false);

  fileMenu->addSeparator();

  close_file_action_ = fileMenu->addAction(QStringLiteral("关闭文件"), this,
                                           &MainWindow::onCloseCurrentFile);
  close_file_action_->setShortcut(QKeySequence::Close);
  close_file_action_->setEnabled(false);

  close_all_files_action_ = fileMenu->addAction(
      QStringLiteral("关闭所有文件"), this, &MainWindow::onCloseAllFiles);
  close_all_files_action_->setEnabled(false);

  fileMenu->addSeparator();

  recent_projects_menu_ = fileMenu->addMenu(QStringLiteral("最近项目"));
  updateRecentProjectsMenu();

  fileMenu->addSeparator();
  fileMenu->addAction(QStringLiteral("退出"), this, &QWidget::close);

  QMenu* edit_menu = menuBar->addMenu(QStringLiteral("编辑(&E)"));
  edit_menu->setObjectName("editMenu");
  view_menu_ = menuBar->addMenu(QStringLiteral("视图(&V)"));

  // 欢迎页
  auto* welcomeViewAction = view_menu_->addAction(QStringLiteral("欢迎页(&W)"));
  connect(welcomeViewAction, &QAction::triggered, this, [this]() {
    auto* centralDock = dock_manager_->findDockWidget("CentralDock");
    if (centralDock && centralDock->dockAreaWidget()) {
      centralDock->dockAreaWidget()->setCurrentIndex(0);
    }
  });

  // 输出面板
  view_panel_action_ = view_menu_->addAction(QStringLiteral("输出面板"));
  view_panel_action_->setShortcut(QStringLiteral("Ctrl+J"));
  view_panel_action_->setCheckable(true);
  view_panel_action_->setChecked(true);
  connect(view_panel_action_, &QAction::triggered, this, [this](bool checked) {
    auto* panelDock = dock_manager_->findDockWidget("PanelDock");
    if (panelDock) {
      if (checked) {
        panelDock->toggleView(true);
        // toggleView会重建标题栏，需要重新隐藏按钮
        hideDockTitleBarButtons(panelDock->dockAreaWidget());
      } else {
        panelDock->closeDockWidget();
      }
    }
  });

  // 辅助侧边栏
  view_aux_sidebar_action_ =
      view_menu_->addAction(QStringLiteral("辅助侧边栏"));
  view_aux_sidebar_action_->setCheckable(true);
  view_aux_sidebar_action_->setChecked(true);
  connect(view_aux_sidebar_action_, &QAction::triggered, this,
          [this](bool checked) {
            auto* auxDock = dock_manager_->findDockWidget("AuxSidebarDock");
            if (auxDock) {
              auxDock->toggleView(checked);
              // toggleView会重建标题栏，需要重新隐藏按钮
              hideDockTitleBarButtons(auxDock->dockAreaWidget());
            }
          });

  auto* toolsMenu = menuBar->addMenu(QStringLiteral("工具(&T)"));
  toolsMenu->addAction(QStringLiteral("设置(&S)..."), this, [this]() {
    if (!settings_dialog_) {
      settings_dialog_ = new SettingsWidget(this);
    }
    settings_dialog_->show();
    settings_dialog_->raise();
    settings_dialog_->activateWindow();
  });

  auto* helpMenu = menuBar->addMenu(QStringLiteral("帮助(&H)"));
  helpMenu->addAction(QStringLiteral("关于(&A)..."), this, [this]() {
    QMessageBox::about(this, QStringLiteral("关于 ETest Demo"),
                       QStringLiteral("ETest Demo v1.0.0\n\n"
                                      "基于 Qt/C++ 的测试系统仿真实现。"));
  });
}

void MainWindow::createStatusBar() {
  // 状态栏样式已由全局QSS覆盖，无需内联设置

  // 左侧区域
  status_message_label_ = new QLabel(this);
  status_message_label_->setText(QStringLiteral("就绪"));
  statusBar()->addWidget(status_message_label_);

  status_project_label_ = new QLabel(this);
  status_project_label_->setText(QStringLiteral("无打开项目"));
  statusBar()->addWidget(status_project_label_);

  status_errors_label_ = new QLabel(this);
  status_errors_label_->setText(QStringLiteral("0 错误, 0 警告"));
  statusBar()->addWidget(status_errors_label_);

  // 右侧区域（addPermanentWidget添加到右侧，顺序从左到右）
  status_language_label_ = new QLabel(this);
  status_language_label_->setText(QStringLiteral("纯文本"));
  statusBar()->addPermanentWidget(status_language_label_);

  status_eol_label_ = new QLabel(this);
  status_eol_label_->setText(QStringLiteral("CRLF"));
  statusBar()->addPermanentWidget(status_eol_label_);

  status_encoding_label_ = new QLabel(this);
  status_encoding_label_->setText(QStringLiteral("UTF-8"));
  statusBar()->addPermanentWidget(status_encoding_label_);

  status_cursor_label_ = new QLabel(this);
  status_cursor_label_->setText(QStringLiteral("行 1, 列 1"));
  statusBar()->addPermanentWidget(status_cursor_label_);

  statusBar()->clearMessage();
}

void MainWindow::createToolBar() {
  // ==================== 文件工具栏 ====================
  file_toolbar_ = addToolBar(QStringLiteral("文件"));
  file_toolbar_->setObjectName("FileToolbar");
  file_toolbar_->setMovable(false);                           // 固定在顶部
  file_toolbar_->setToolButtonStyle(Qt::ToolButtonIconOnly);  // 仅显示图标

  new_project_action_->setIcon(style()->standardIcon(QStyle::SP_FileIcon));
  new_project_action_->setToolTip(QStringLiteral("新建项目 (Ctrl+Shift+N)"));
  file_toolbar_->addAction(new_project_action_);

  open_project_action_->setIcon(
      style()->standardIcon(QStyle::SP_DialogOpenButton));
  open_project_action_->setToolTip(QStringLiteral("打开项目 (Ctrl+O)"));
  file_toolbar_->addAction(open_project_action_);

  file_toolbar_->addSeparator();

  save_action_->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
  save_action_->setToolTip(QStringLiteral("保存 (Ctrl+S)"));
  file_toolbar_->addAction(save_action_);

  // ==================== 编辑工具栏 ====================
  edit_toolbar_ = addToolBar(QStringLiteral("编辑"));
  edit_toolbar_->setObjectName("EditToolbar");
  edit_toolbar_->setMovable(false);
  edit_toolbar_->setToolButtonStyle(Qt::ToolButtonIconOnly);

  edit_undo_action_->setIcon(style()->standardIcon(QStyle::SP_ArrowBack));
  edit_undo_action_->setToolTip(QStringLiteral("撤销 (Ctrl+Z)"));
  edit_toolbar_->addAction(edit_undo_action_);

  edit_redo_action_->setIcon(style()->standardIcon(QStyle::SP_ArrowForward));
  edit_redo_action_->setToolTip(QStringLiteral("重做 (Ctrl+Y)"));
  edit_toolbar_->addAction(edit_redo_action_);

  edit_toolbar_->addSeparator();

  edit_cut_action_->setIcon(style()->standardIcon(QStyle::SP_FileLinkIcon));
  edit_cut_action_->setToolTip(QStringLiteral("剪切 (Ctrl+X)"));
  edit_toolbar_->addAction(edit_cut_action_);

  edit_copy_action_->setIcon(style()->standardIcon(QStyle::SP_FileIcon));
  edit_copy_action_->setToolTip(QStringLiteral("复制 (Ctrl+C)"));
  edit_toolbar_->addAction(edit_copy_action_);

  edit_paste_action_->setIcon(style()->standardIcon(QStyle::SP_FileLinkIcon));
  edit_paste_action_->setToolTip(QStringLiteral("粘贴 (Ctrl+V)"));
  edit_toolbar_->addAction(edit_paste_action_);
}

void MainWindow::createEditMenu() {
  QMenu* edit_menu = menuBar()->findChild<QMenu*>("editMenu");
  if (!edit_menu) {
    edit_menu = menuBar()->addMenu(QStringLiteral("编辑(&E)"));
    edit_menu->setObjectName("editMenu");
  }

  // 撤销
  edit_undo_action_ =
      edit_menu->addAction(style()->standardIcon(QStyle::SP_ArrowBack),
                           QStringLiteral("撤销"), this, &MainWindow::onUndo);
  edit_undo_action_->setShortcut(QKeySequence::Undo);
  edit_undo_action_->setEnabled(false);

  // 重做
  edit_redo_action_ =
      edit_menu->addAction(style()->standardIcon(QStyle::SP_ArrowForward),
                           QStringLiteral("重做"), this, &MainWindow::onRedo);
  edit_redo_action_->setShortcut(QKeySequence::Redo);
  edit_redo_action_->setEnabled(false);

  edit_menu->addSeparator();

  // 剪切
  edit_cut_action_ =
      edit_menu->addAction(QStringLiteral("剪切"), this, &MainWindow::onCut);
  edit_cut_action_->setShortcut(QKeySequence::Cut);
  edit_cut_action_->setEnabled(false);

  // 复制
  edit_copy_action_ =
      edit_menu->addAction(QStringLiteral("复制"), this, &MainWindow::onCopy);
  edit_copy_action_->setShortcut(QKeySequence::Copy);
  edit_copy_action_->setEnabled(false);

  // 粘贴
  edit_paste_action_ =
      edit_menu->addAction(QStringLiteral("粘贴"), this, &MainWindow::onPaste);
  edit_paste_action_->setShortcut(QKeySequence::Paste);
  edit_paste_action_->setEnabled(false);

  edit_menu->addSeparator();

  // 查找
  edit_find_action_ =
      edit_menu->addAction(QStringLiteral("查找"), this, &MainWindow::onFind);
  edit_find_action_->setShortcut(QKeySequence::Find);
  edit_find_action_->setEnabled(false);

  // 替换
  edit_replace_action_ = edit_menu->addAction(QStringLiteral("替换"), this,
                                              &MainWindow::onReplace);
  edit_replace_action_->setShortcut(QKeySequence::Replace);
  edit_replace_action_->setEnabled(false);

  edit_menu->addSeparator();

  // 跳转到行
  edit_go_to_line_action_ = edit_menu->addAction(QStringLiteral("跳转到行"),
                                                 this, &MainWindow::onGoToLine);
  edit_go_to_line_action_->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_G));
  edit_go_to_line_action_->setEnabled(false);
}

void MainWindow::onNewProject() {
  // 先尝试关闭当前项目
  if (!tryCloseCurrentProject()) {
    return;  // 用户取消，不继续创建
  }

  etest::app::NewProjectDialog dlg(this);
  if (dlg.exec() == QDialog::Accepted) {
    auto& projectMgr = etest::core::project::ProjectManager::instance();
    if (!projectMgr.createProject(dlg.projectName(), dlg.projectLocation())) {
      QMessageBox::warning(this, QStringLiteral("新建项目失败"),
                           QStringLiteral("无法创建项目，请检查名称和路径。"));
    }
  }
}

void MainWindow::onOpenProject() {
  // 先尝试关闭当前项目
  if (!tryCloseCurrentProject()) {
    return;  // 用户取消，不继续打开
  }

  auto& cfg = ConfigManager::instance();
  QString lastPath = cfg.get<QString>(CONFIG_RECENT_LAST_OPEN_PATH);
  if (lastPath.isEmpty()) {
    lastPath = QDir::homePath();
  }

  QString filePath =
      QFileDialog::getOpenFileName(this, QStringLiteral("打开项目"), lastPath,
                                   QStringLiteral("ETest项目文件 (*.etproj)"));

  if (!filePath.isEmpty()) {
    auto& projectMgr = etest::core::project::ProjectManager::instance();
    if (!projectMgr.openProject(filePath)) {
      QMessageBox::warning(
          this, QStringLiteral("打开项目失败"),
          QStringLiteral("无法打开项目文件：%1").arg(filePath));
    }
  }
}

void MainWindow::openRecentProject(const QString& path) {
  if (!tryCloseCurrentProject()) {
    return;
  }

  auto& pm = etest::core::project::ProjectManager::instance();
  if (pm.openProject(path))
    return;

  QFileInfo fi(path);
  QString msg = fi.exists()
      ? QStringLiteral("无法打开项目文件：%1").arg(path)
      : QStringLiteral("项目文件 \"%1\" 不存在，\n文件可能已被移动或删除。\n\n是否从最近项目中移除此记录？").arg(path);

  auto buttons = fi.exists()
      ? QMessageBox::Ok
      : (QMessageBox::Yes | QMessageBox::No);

  if (fi.exists()) {
    QMessageBox::warning(this, QStringLiteral("打开项目失败"), msg);
  } else {
    int ret = QMessageBox::question(this, QStringLiteral("打开项目失败"), msg,
                                    buttons);
    if (ret == QMessageBox::Yes) {
      pm.removeFromRecentProjects(path);
    }
  }
}

bool MainWindow::tryCloseCurrentProject() {
  auto& projectMgr = etest::core::project::ProjectManager::instance();
  if (!projectMgr.isProjectOpen())
    return true;

  QString projectRoot = projectMgr.currentProjectRoot();
  if (editor_manager_->hasUnsavedChangesInDirectory(projectRoot)) {
    QString message = QStringLiteral("项目中有未保存的文件更改，是否保存？");

    int ret = QMessageBox::question(
        this, QStringLiteral("保存更改"), message,
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

    if (ret == QMessageBox::Cancel)
      return false;

    if (ret == QMessageBox::Yes) {
      // 只保存项目内的未保存文件
      if (!editor_manager_->saveModifiedFilesInDirectory(projectRoot)) {
        QMessageBox::warning(
            this, QStringLiteral("保存失败"),
            QStringLiteral("部分文件保存失败，无法关闭项目。"));
        return false;
      }
    }
  }

  // 先关闭项目内的所有文件，如果用户取消关闭文件，则不关闭项目
  if (!editor_manager_->closeFilesInDirectory(projectRoot)) {
    return false;
  }

  // 文件都关闭成功后，再关闭项目
  projectMgr.closeProject();
  return true;
}

void MainWindow::onCloseProject() {
  tryCloseCurrentProject();
}

void MainWindow::onProjectOpened(const QString& projectPath) {
  close_project_action_->setEnabled(true);

  auto& projectMgr = etest::core::project::ProjectManager::instance();
  auto* project = projectMgr.currentProject();
  if (project) {
    status_project_label_->setText(project->name());
  }

  updateWindowTitle();
  status_message_label_->setText(
      QStringLiteral("项目已打开：%1").arg(projectPath));
}

void MainWindow::onProjectClosed() {
  close_project_action_->setEnabled(false);
  status_project_label_->setText(QStringLiteral("无打开项目"));
  updateWindowTitle();
  status_message_label_->setText(QStringLiteral("项目已关闭"));
}

void MainWindow::updateWindowTitle() {
  auto& projectMgr = etest::core::project::ProjectManager::instance();
  if (projectMgr.isProjectOpen()) {
    auto* project = projectMgr.currentProject();
    QString title = project->name();
    if (projectMgr.hasUnsavedChanges()) {
      title.prepend("* ");
    }
    title += " - ETest Demo";
    setWindowTitle(title);
  } else {
    setWindowTitle(QStringLiteral("ETest Demo"));
  }
}

void MainWindow::updateRecentProjectsMenu() {
  recent_projects_menu_->clear();

  auto& projectMgr = etest::core::project::ProjectManager::instance();
  QStringList recentList = projectMgr.recentProjects();

  if (recentList.isEmpty()) {
    auto* emptyAction =
        recent_projects_menu_->addAction(QStringLiteral("（无）"));
    emptyAction->setEnabled(false);
  } else {
    for (const QString& path : recentList) {
      recent_projects_menu_->addAction(path, this, [this, path]() {
        openRecentProject(path);
      });
    }

    recent_projects_menu_->addSeparator();
    recent_projects_menu_->addAction(
        QStringLiteral("清除最近项目"), this, [this]() {
          etest::core::project::ProjectManager::instance()
              .clearRecentProjects();
        });
  }
}

void MainWindow::onSaveFile() {
  auto* editor = editor_manager_->currentEditor();
  if (!editor)
    return;
  if (!editor->save()) {
    QMessageBox::warning(
        this, QStringLiteral("保存失败"),
        QStringLiteral("无法保存文件：%1").arg(editor->filePath()));
  }
}

void MainWindow::onSaveFileAs() {
  auto* editor = editor_manager_->currentEditor();
  if (!editor)
    return;

  QString newPath = QFileDialog::getSaveFileName(
      this, QStringLiteral("另存为"), editor->filePath(),
      QStringLiteral("所有文件 (*)"));
  if (!newPath.isEmpty()) {
    if (!editor->saveAs(newPath)) {
      QMessageBox::warning(this, QStringLiteral("保存失败"),
                           QStringLiteral("无法保存文件：%1").arg(newPath));
    }
  }
}

void MainWindow::onSaveAllFiles() {
  editor_manager_->saveAllFiles();
}

void MainWindow::onCloseCurrentFile() {
  auto* editor = editor_manager_->currentEditor();
  if (!editor)
    return;
  editor_manager_->closeFile(editor->editorId());
}

void MainWindow::onCloseAllFiles() {
  editor_manager_->closeAllFiles();
}

void MainWindow::onUndo() {
  if (auto* editor = editor_manager_->currentEditor()) {
    editor->undo();
  }
}

void MainWindow::onRedo() {
  if (auto* editor = editor_manager_->currentEditor()) {
    editor->redo();
  }
}

void MainWindow::onCut() {
  if (auto* editor = editor_manager_->currentEditor()) {
    if (auto* textEditor = dynamic_cast<TextEditorWidget*>(editor)) {
      textEditor->editor()->cut();
    }
  }
}

void MainWindow::onCopy() {
  if (auto* editor = editor_manager_->currentEditor()) {
    if (auto* textEditor = dynamic_cast<TextEditorWidget*>(editor)) {
      textEditor->editor()->copy();
    }
  }
}

void MainWindow::onPaste() {
  if (auto* editor = editor_manager_->currentEditor()) {
    if (auto* textEditor = dynamic_cast<TextEditorWidget*>(editor)) {
      textEditor->editor()->paste();
    }
  }
}

void MainWindow::onFind() {
  auto* editor = editor_manager_->currentEditor();
  auto* textEditor = dynamic_cast<TextEditorWidget*>(editor);
  if (!textEditor)
    return;

  bool ok;
  QString searchText = QInputDialog::getText(this, QStringLiteral("查找"),
                                             QStringLiteral("查找内容:"),
                                             QLineEdit::Normal, QString(), &ok);
  if (ok && !searchText.isEmpty()) {
    int line, column;
    textEditor->editor()->getCursorPosition(&line, &column);

    bool found = textEditor->editor()->findFirst(
        searchText, false, false, false, true, true, line, column, true);
    if (!found) {
      QMessageBox::information(this, QStringLiteral("查找"),
                               QStringLiteral("找不到指定内容"));
    }
  }
}

void MainWindow::onReplace() {
  auto* editor = editor_manager_->currentEditor();
  auto* textEditor = dynamic_cast<TextEditorWidget*>(editor);
  if (!textEditor)
    return;

  bool ok;
  QString searchText = QInputDialog::getText(this, QStringLiteral("替换"),
                                             QStringLiteral("查找内容:"),
                                             QLineEdit::Normal, QString(), &ok);
  if (!ok || searchText.isEmpty())
    return;

  QString replaceText = QInputDialog::getText(
      this, QStringLiteral("替换"), QStringLiteral("替换为:"),
      QLineEdit::Normal, QString(), &ok);
  if (!ok)
    return;

  int line, column;
  textEditor->editor()->getCursorPosition(&line, &column);

  bool found = textEditor->editor()->findFirst(searchText, false, false, false,
                                               true, true, line, column, true);
  if (found) {
    int ret = QMessageBox::question(
        this, QStringLiteral("替换"), QStringLiteral("替换当前匹配项吗？"),
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
    if (ret == QMessageBox::Cancel) {
      return;
    } else if (ret == QMessageBox::Yes) {
      textEditor->editor()->replace(replaceText);
    }

    while (textEditor->editor()->findNext()) {
      ret = QMessageBox::question(
          this, QStringLiteral("替换"), QStringLiteral("替换当前匹配项吗？"),
          QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
      if (ret == QMessageBox::Cancel) {
        break;
      } else if (ret == QMessageBox::Yes) {
        textEditor->editor()->replace(replaceText);
      }
    }

    QMessageBox::information(this, QStringLiteral("替换"),
                             QStringLiteral("替换完成"));
  } else {
    QMessageBox::information(this, QStringLiteral("替换"),
                             QStringLiteral("找不到指定内容"));
  }
}

void MainWindow::onGoToLine() {
  auto* editor = editor_manager_->currentEditor();
  auto* textEditor = dynamic_cast<TextEditorWidget*>(editor);
  if (!textEditor)
    return;

  int lineCount = textEditor->editor()->lines();
  bool ok;

  int currentLine, currentColumn;
  textEditor->editor()->getCursorPosition(&currentLine, &currentColumn);

  int lineNumber =
      QInputDialog::getInt(this, QStringLiteral("跳转到行"),
                           QStringLiteral("行号 (1-%1):").arg(lineCount),
                           currentLine + 1, 1, lineCount, 1, &ok);
  if (ok) {
    textEditor->editor()->setCursorPosition(lineNumber - 1, 0);
    textEditor->editor()->ensureLineVisible(lineNumber - 1);
  }
}

void MainWindow::closeEvent(QCloseEvent* event) {
  // 捕获当前会话状态（文件都还开着，数据完整）
  QJsonObject sessionData = captureSessionData();

  // 尝试关闭所有编辑器文件，如果用户取消则不关闭程序
  if (!editor_manager_->closeAllFiles()) {
    event->ignore();
    // sessionData 出作用域自动释放
    return;
  }

  // 关闭项目
  auto& projectMgr = etest::core::project::ProjectManager::instance();
  if (projectMgr.isProjectOpen()) {
    projectMgr.closeProject();
  }

  // 确认关闭后才写盘
  writeSessionFile(sessionData);
  saveWindowState();
  QMainWindow::closeEvent(event);
}

void MainWindow::saveWindowState() {
  auto& cfg = ConfigManager::instance();
  cfg.set(CONFIG_WINDOW_WIDTH, width());
  cfg.set(CONFIG_WINDOW_HEIGHT, height());
  cfg.set(CONFIG_WINDOW_X, x());
  cfg.set(CONFIG_WINDOW_Y, y());
  cfg.set(CONFIG_WINDOW_MAXIMIZED, isMaximized());

  // 保存工具栏可见性
  if (file_toolbar_) {
    cfg.set(CONFIG_TOOLBAR_VISIBLE, file_toolbar_->isVisible());
  }

  QByteArray dockState = dock_manager_->saveState();
  cfg.set(CONFIG_DOCK_LAYOUT, QString(dockState.toBase64()));
}

void MainWindow::restoreWindowState() {
  auto& cfg = ConfigManager::instance();

  int w = cfg.get<int>(CONFIG_WINDOW_WIDTH, CONFIG_WINDOW_DEFAULT_WIDTH);
  int h = cfg.get<int>(CONFIG_WINDOW_HEIGHT, CONFIG_WINDOW_DEFAULT_HEIGHT);
  resize(w, h);

  int x = cfg.get<int>(CONFIG_WINDOW_X, CONFIG_WINDOW_DEFAULT_X);
  int y = cfg.get<int>(CONFIG_WINDOW_Y, CONFIG_WINDOW_DEFAULT_Y);
  if (x >= 0 && y >= 0) {
    move(x, y);
  }

  if (cfg.get<bool>(CONFIG_WINDOW_MAXIMIZED, CONFIG_WINDOW_DEFAULT_MAXIMIZED)) {
    showMaximized();
  }

  QString dockStateStr = cfg.get<QString>(CONFIG_DOCK_LAYOUT);
  if (!dockStateStr.isEmpty()) {
    QByteArray dockState = QByteArray::fromBase64(dockStateStr.toUtf8());
    dock_manager_->restoreState(dockState);
  }

  // restoreState会重建标题栏，需要重新隐藏固定dock的标题栏
  if (sidebar_dock_ && sidebar_dock_->dockAreaWidget()) {
    sidebar_dock_->dockAreaWidget()->titleBar()->hide();
  }
  auto* panelDock = dock_manager_->findDockWidget("PanelDock");
  if (panelDock && panelDock->dockAreaWidget()) {
    hideDockTitleBarButtons(panelDock->dockAreaWidget());
  }
  auto* auxDock = dock_manager_->findDockWidget("AuxSidebarDock");
  if (auxDock && auxDock->dockAreaWidget()) {
    hideDockTitleBarButtons(auxDock->dockAreaWidget());
  }

  // 恢复工具栏可见性
  if (file_toolbar_ && edit_toolbar_) {
    bool toolbarVisible =
        cfg.get<bool>(CONFIG_TOOLBAR_VISIBLE, CONFIG_TOOLBAR_DEFAULT_VISIBLE);
    file_toolbar_->setVisible(toolbarVisible);
    edit_toolbar_->setVisible(toolbarVisible);
  }
}

QJsonObject MainWindow::captureSessionData() {
  QJsonObject root;
  root["version"] = 1;

  // 当前项目路径
  auto& projectMgr = etest::core::project::ProjectManager::instance();
  if (projectMgr.isProjectOpen()) {
    root["projectPath"] = projectMgr.currentProject()->projectFilePath();
  }

  // 编辑器状态
  QJsonObject editorsObj;
  editorsObj["activeFile"] = editor_manager_->currentFilePath();

  QJsonArray filesArray;
  for (const QString& path : editor_manager_->openFiles()) {
    QJsonObject fileObj;
    fileObj["path"] = path;
    auto* editor = editor_manager_->editorById(path);
    if (auto* textEditor = dynamic_cast<TextEditorWidget*>(editor)) {
      int line, col;
      textEditor->editor()->getCursorPosition(&line, &col);
      fileObj["cursorLine"] = line;
      fileObj["cursorColumn"] = col;
      fileObj["scrollPos"] = textEditor->editor()->verticalScrollBar()->value();
    }
    filesArray.append(fileObj);
  }
  editorsObj["openFiles"] = filesArray;
  root["editors"] = editorsObj;

  // 侧边栏状态
  QJsonObject sidebarObj;
  int tab = sidebar_->activeIndex();
  if (tab >= 0 && tab < sidebar_->pageCount()) {
    sidebarObj["activeTab"] = tab;
  } else {
    sidebarObj["activeTab"] = 0;
  }
  sidebarObj["visible"] = sidebar_dock_ && !sidebar_dock_->isClosed();
  root["sidebar"] = sidebarObj;

  // 面板状态
  QJsonObject panelObj;
  if (panel_container_) {
    panelObj["activeTab"] = panel_container_->currentPanelIndex();
    panelObj["maximized"] = panel_container_->isMaximized();
  }
  auto* panelDock = dock_manager_->findDockWidget("PanelDock");
  panelObj["visible"] = panelDock && !panelDock->isClosed();
  root["panel"] = panelObj;

  int fileCount = editorsObj["openFiles"].toArray().size();
  LOG_INFO("SESSION", "会话已捕获：项目={}, 文件={}, 侧边栏tab={}, 面板tab={}",
           root.contains("projectPath")
               ? root["projectPath"].toString().toStdString()
               : "无",
           fileCount, sidebarObj["activeTab"].toInt(),
           panelObj["activeTab"].toInt());
  return root;
}

void MainWindow::writeSessionFile(const QJsonObject& data) {
  QString sessionPath =
      QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) +
      "/session.json";
  QFile file(sessionPath);
  if (file.open(QIODevice::WriteOnly)) {
    qint64 bytes = file.write(QJsonDocument(data).toJson());
    LOG_INFO("SESSION", "会话已写入：{} ({} 字节)", sessionPath.toStdString(),
             bytes);
  } else {
    LOG_WARN("SESSION", "会话写入失败：无法打开 {}", sessionPath.toStdString());
  }
}

void MainWindow::restoreSession() {
  QString sessionPath =
      QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) +
      "/session.json";
  QFile file(sessionPath);
  if (!file.open(QIODevice::ReadOnly))
    return;

  QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  QJsonObject root = doc.object();
  if (root["version"].toInt() != 1)
    return;

  // 恢复侧边栏
  QJsonObject sidebarObj = root["sidebar"].toObject();
  if (!sidebarObj.isEmpty()) {
    int tab = sidebarObj["activeTab"].toInt();
    if (tab >= 0 && tab < sidebar_->pageCount()) {
      sidebar_->setActiveIndex(tab);
      sidebar_->switchPage(tab);
    } else {
      sidebar_->setActiveIndex(0);
      sidebar_->switchPage(0);
    }

    if (sidebar_dock_) {
      if (sidebarObj["visible"].toBool(true) && sidebar_dock_->isClosed()) {
        sidebar_dock_->toggleView(true);
        if (sidebar_dock_->dockAreaWidget()) {
          sidebar_dock_->dockAreaWidget()->titleBar()->hide();
        }
      } else if (!sidebarObj["visible"].toBool(true) &&
                 !sidebar_dock_->isClosed()) {
        sidebar_dock_->closeDockWidget();
      }
    }
  }

  // 恢复面板
  QJsonObject panelObj = root["panel"].toObject();
  if (!panelObj.isEmpty() && panel_container_) {
    panel_container_->setCurrentPanel(panelObj["activeTab"].toInt());

    auto* panelDock = dock_manager_->findDockWidget("PanelDock");
    if (panelDock) {
      if (panelObj["visible"].toBool(true) && panelDock->isClosed()) {
        panelDock->toggleView(true);
        hideDockTitleBarButtons(panelDock->dockAreaWidget());
      } else if (!panelObj["visible"].toBool(true) && !panelDock->isClosed()) {
        panelDock->closeDockWidget();
      }
    }

    if (panelObj["maximized"].toBool()) {
      panel_container_->setMaximized(true);
      if (sidebar_dock_)
        sidebar_dock_->closeDockWidget();
    }
  }

  // 恢复项目
  QString projectPath = root["projectPath"].toString();
  if (!projectPath.isEmpty() && QFileInfo::exists(projectPath)) {
    etest::core::project::ProjectManager::instance().openProject(projectPath);
  }

  // 恢复编辑器（先恢复所有文件，最后激活 activeFile）
  QJsonObject editorsObj = root["editors"].toObject();
  QJsonArray filesArray = editorsObj["openFiles"].toArray();
  QString activeFile = editorsObj["activeFile"].toString();

  for (const QJsonValue& val : filesArray) {
    QJsonObject fileObj = val.toObject();
    QString path = fileObj["path"].toString();
    if (path.isEmpty() || !QFileInfo::exists(path))
      continue;

    editor_manager_->openFile(path);

    auto* editor = editor_manager_->editorById(path);
    if (auto* textEditor = dynamic_cast<TextEditorWidget*>(editor)) {
      int line = fileObj["cursorLine"].toInt();
      int col = fileObj["cursorColumn"].toInt();
      int scrollPos = fileObj["scrollPos"].toInt();

      int maxLine = textEditor->editor()->lines() - 1;
      if (line > maxLine)
        line = maxLine;
      if (line < 0)
        line = 0;

      int maxCol = textEditor->editor()->text(line).length();
      if (col > maxCol)
        col = maxCol;
      if (col < 0)
        col = 0;

      textEditor->editor()->setCursorPosition(line, col);

      int maxScroll = textEditor->editor()->verticalScrollBar()->maximum();
      if (scrollPos > maxScroll)
        scrollPos = maxScroll;
      if (scrollPos < 0)
        scrollPos = 0;
      textEditor->editor()->verticalScrollBar()->setValue(scrollPos);
    }
  }

  // 激活之前正在编辑的文件（已在循环中打开，只需 raise）
  if (!activeFile.isEmpty() && editor_manager_->isOpen(activeFile)) {
    // 查找对应的 dock 并置前
    editor_manager_->openFile(activeFile);
  }

  int restoredCount = filesArray.size();
  int skippedCount = 0;
  for (const QJsonValue& val : filesArray) {
    if (!QFileInfo::exists(val.toObject()["path"].toString()))
      skippedCount++;
  }
  LOG_INFO("SESSION", "会话恢复完成：文件={}, 跳过={}, 项目={}",
           restoredCount - skippedCount, skippedCount,
           projectPath.isEmpty() ? "无" : projectPath.toStdString());
}

void MainWindow::hideDockTitleBarButtons(ads::CDockAreaWidget* area) {
  if (!area)
    return;
  auto* titleBar = area->titleBar();
  if (!titleBar)
    return;
  for (auto* btn : titleBar->findChildren<QToolButton*>()) {
    auto name = btn->objectName();
    if (name == "tabsMenuButton" || name == "detachGroupButton" ||
        name == "dockAreaCloseButton") {
      btn->hide();
    }
  }
}

}  // namespace etest::app