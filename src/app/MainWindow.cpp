#include "MainWindow.h"

#include <QClipboard>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QFile>
#include <QFileDialog>
#include <QGuiApplication>
#include <QInputDialog>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QShortcut>
#include <QStatusBar>
#include <QToolButton>


#include "ActivityBarWidget.h"
#include "EditorManager.h"
#include "EditorWidget.h"
#include "FileExplorerWidget.h"
#include "HardwareTreeWidget.h"
#include "OutputPanel.h"
#include "PanelContainerWidget.h"
#include "ProblemsPanel.h"
#include "SidebarWidget.h"
#include "TerminalPanel.h"

#include <DockAreaTitleBar.h>
#include <DockAreaWidget.h>
#include "config/ConfigDefs.h"
#include "config/ConfigManager.h"
#include "dialogs/NewProjectDialog.h"
#include "logger/Logger.h"
#include "logger/QtConsoleSink.h"
#include "plugin/PluginManager.h"
#include "project/ProjectManager.h"


using namespace etest::core::config;
using namespace etest::core::project;
using namespace etest::core::logger;

namespace etest::app {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      dock_manager_(nullptr),
      activity_bar_(nullptr),
      sidebar_(nullptr),
      editor_manager_(nullptr),
      output_panel_(nullptr),
      problems_panel_(nullptr),
      terminal_panel_(nullptr) {
  initUi();
  initSignals();
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
  dock_manager_ = new ads::CDockManager(this);

  // 覆盖QADS内置的default.css，应用暗色主题（必须设置到CDockManager自身才生效）
  QFile adsStyleFile(":/resources/styles/ads_dark.qss");
  if (adsStyleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
    dock_manager_->setStyleSheet(dock_manager_->styleSheet() +
                                 QString::fromUtf8(adsStyleFile.readAll()));
    adsStyleFile.close();
  }

  // 中央编辑区（必须在添加其他dock之前建立）
  auto* centralPlaceholder = new QWidget(this);
  auto* centralDock = new ads::CDockWidget(QStringLiteral("中央编辑区"));
  centralDock->setObjectName("CentralDock");
  centralDock->setWidget(centralPlaceholder);
  dock_manager_->setCentralWidget(centralDock);

  // 编辑器管理器
  editor_manager_ = new EditorManager(dock_manager_, this);

  // ==================== 左侧：侧边栏（先添加，占据左侧区域）
  // ====================
  sidebar_ = new SidebarWidget(this);
  sidebar_dock_ = new ads::CDockWidget(QStringLiteral("侧边栏"));
  sidebar_dock_->setObjectName("SidebarDock");
  sidebar_dock_->setWidget(sidebar_);
  sidebar_dock_->setFeature(ads::CDockWidget::DockWidgetClosable, false);
  sidebar_dock_->setFeature(ads::CDockWidget::DockWidgetFloatable, false);
  sidebar_dock_->setFeature(ads::CDockWidget::DockWidgetMovable, false);
  dock_manager_->addDockWidget(ads::LeftDockWidgetArea, sidebar_dock_);
  // 隐藏侧边栏标题栏
  sidebar_dock_->dockAreaWidget()->titleBar()->hide();

  // ==================== 左侧：活动栏（侧边栏左侧） ====================
  activity_bar_ = new ActivityBarWidget(this);
  auto* activityDock = new ads::CDockWidget(QStringLiteral("活动栏"));
  activityDock->setObjectName("ActivityDock");
  activityDock->setWidget(activity_bar_);
  activityDock->setFeature(ads::CDockWidget::DockWidgetClosable, false);
  activityDock->setFeature(ads::CDockWidget::DockWidgetFloatable, false);
  activityDock->setFeature(ads::CDockWidget::DockWidgetMovable, false);
  // 活动栏放置在侧边栏左侧
  dock_manager_->addDockWidget(ads::LeftDockWidgetArea, activityDock,
                               sidebar_dock_->dockAreaWidget());
  // 隐藏活动栏标题栏
  activityDock->dockAreaWidget()->titleBar()->hide();

  // ==================== 底部：统一面板容器 ====================
  output_panel_ = new OutputPanel(this);
  problems_panel_ = new ProblemsPanel(this);
  terminal_panel_ = new TerminalPanel(this);

  auto* panelContainer = new PanelContainerWidget(this);
  panelContainer->addPanel(QStringLiteral("输出"), output_panel_);
  panelContainer->addPanel(QStringLiteral("问题"), problems_panel_);
  panelContainer->addPanel(QStringLiteral("终端"), terminal_panel_);

  auto* panelDock = new ads::CDockWidget(QStringLiteral("面板"));
  panelDock->setObjectName("PanelDock");
  panelDock->setWidget(panelContainer);
  panelDock->setFeature(ads::CDockWidget::DockWidgetClosable, true);
  panelDock->setFeature(ads::CDockWidget::DockWidgetFloatable, false);
  panelDock->setFeature(ads::CDockWidget::DockWidgetMovable, false);
  dock_manager_->addDockWidget(ads::BottomDockWidgetArea, panelDock);
  // 隐藏面板标题栏右侧的三个按钮（PanelContainerWidget内部已有tab和关闭按钮）
  hideDockTitleBarButtons(panelDock->dockAreaWidget());

  // 面板容器信号
  connect(panelContainer, &PanelContainerWidget::panelClosed, this,
          [panelDock]() { panelDock->closeDockWidget(); });
  connect(panelContainer, &PanelContainerWidget::panelMaximized, this, [this]() {
    if (sidebar_dock_) sidebar_dock_->closeDockWidget();
    auto* activityDock = dock_manager_->findDockWidget("ActivityDock");
    if (activityDock) activityDock->closeDockWidget();
  });
  connect(panelContainer, &PanelContainerWidget::panelRestored, this, [this]() {
    if (sidebar_dock_) {
      sidebar_dock_->toggleView(true);
      if (sidebar_dock_->dockAreaWidget()) {
        sidebar_dock_->dockAreaWidget()->titleBar()->hide();
      }
    }
    auto* activityDock = dock_manager_->findDockWidget("ActivityDock");
    if (activityDock) {
      activityDock->toggleView(true);
      if (activityDock->dockAreaWidget()) {
        activityDock->dockAreaWidget()->titleBar()->hide();
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

  // 活动栏切换侧边栏
  connect(activity_bar_, &ActivityBarWidget::activityClicked, sidebar_,
          &SidebarWidget::switchPage);
  // 活动栏再次点击已选中按钮时切换侧边栏显隐
  connect(activity_bar_, &ActivityBarWidget::sidebarToggleRequested, this,
          [this]() {
            if (sidebar_dock_) {
              sidebar_dock_->toggleView(sidebar_dock_->isClosed());
              // toggleView会重建标题栏，需要重新隐藏
              if (sidebar_dock_->dockAreaWidget()) {
                sidebar_dock_->dockAreaWidget()->titleBar()->hide();
              }
              auto* activityDock =
                  dock_manager_->findDockWidget("ActivityDock");
              if (activityDock && activityDock->dockAreaWidget()) {
                activityDock->dockAreaWidget()->titleBar()->hide();
              }
            }
          });

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

  // 编辑器：当前编辑器切换时更新状态栏和菜单状态
  connect(
      editor_manager_, &EditorManager::currentEditorChanged, this,
      [this](EditorWidget* editor) {
        bool hasEditor = (editor != nullptr);
        bool hasSelection = false;

        save_as_action_->setEnabled(hasEditor);
        close_file_action_->setEnabled(hasEditor);
        close_all_files_action_->setEnabled(hasEditor);

        // 保存按钮仅在当前文件有修改时启用
        bool isModified = hasEditor && editor->isModified();
        save_action_->setEnabled(isModified);

        if (hasEditor) {
          QsciScintilla* sci_editor = editor->editor();
          hasSelection = sci_editor->hasSelectedText();

          status_message_label_->setText(editor->filePath());

          // 更新状态栏光标位置
          int line, col;
          sci_editor->getCursorPosition(&line, &col);
          status_cursor_label_->setText(
              QStringLiteral("行 %1, 列 %2").arg(line + 1).arg(col + 1));
          // 更新状态栏语言模式
          status_language_label_->setText(QStringLiteral("纯文本"));
          // 更新EOL格式
          status_eol_label_->setText(QStringLiteral("CRLF"));
          status_encoding_label_->setText(QStringLiteral("UTF-8"));

          // 断开之前编辑器的所有信号连接
          QObject::disconnect(current_editor_selection_connection_);
          QObject::disconnect(current_editor_state_connection_);

          // 连接新编辑器的选择变化信号
          current_editor_selection_connection_ =
              connect(sci_editor, &QsciScintilla::selectionChanged, this,
                      [this, sci_editor]() {
                        bool hasSelection = sci_editor->hasSelectedText();
                        edit_cut_action_->setEnabled(hasSelection);
                        edit_copy_action_->setEnabled(hasSelection);
                      });

          // 连接编辑器状态变化信号，更新撤销/重做按钮状态
          current_editor_state_connection_ = connect(
              editor, &EditorWidget::editorStateChanged, this,
              [this, sci_editor]() {
                edit_undo_action_->setEnabled(sci_editor->isUndoAvailable());
                edit_redo_action_->setEnabled(sci_editor->isRedoAvailable());
              });

          // 初始化撤销/重做按钮状态
          edit_undo_action_->setEnabled(sci_editor->isUndoAvailable());
          edit_redo_action_->setEnabled(sci_editor->isRedoAvailable());
        } else {
          status_message_label_->setText(QStringLiteral("就绪"));
          status_cursor_label_->setText(QStringLiteral("行 1, 列 1"));
          status_language_label_->setText(QStringLiteral("纯文本"));
          status_eol_label_->setText(QStringLiteral("CRLF"));
          status_encoding_label_->setText(QStringLiteral("UTF-8"));
          // 没有编辑器时，断开旧连接并禁用相关按钮
          QObject::disconnect(current_editor_selection_connection_);
          QObject::disconnect(current_editor_state_connection_);
          edit_cut_action_->setEnabled(false);
          edit_copy_action_->setEnabled(false);
          edit_undo_action_->setEnabled(false);
          edit_redo_action_->setEnabled(false);
          edit_find_action_->setEnabled(false);
          edit_replace_action_->setEnabled(false);
          edit_go_to_line_action_->setEnabled(false);
        }
        updateWindowTitle();

        // 更新编辑菜单按钮状态（工具栏会自动同步）
        edit_undo_action_->setEnabled(hasEditor);
        edit_redo_action_->setEnabled(hasEditor);
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
  EditorWidget* current_editor = editor_manager_->currentEditor();
  bool hasEditor = (current_editor != nullptr);
  bool hasSelection = false;

  if (hasEditor) {
    QsciScintilla* sci_editor = current_editor->editor();
    hasSelection = sci_editor->hasSelectedText();
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
    if (sidebar_dock_) {
      sidebar_dock_->toggleView(sidebar_dock_->isClosed());
      // toggleView会重建标题栏，需要重新隐藏
      if (sidebar_dock_->dockAreaWidget()) {
        sidebar_dock_->dockAreaWidget()->titleBar()->hide();
      }
      auto* activityDock = dock_manager_->findDockWidget("ActivityDock");
      if (activityDock && activityDock->dockAreaWidget()) {
        activityDock->dockAreaWidget()->titleBar()->hide();
      }
    }
  });

  // 硬件树：插件加载/卸载时自动刷新
  auto* hardwareTree = sidebar_->hardwareTree();
  auto& pluginMgr = etest::core::plugin::PluginManager::instance();
  connect(&pluginMgr, &etest::core::plugin::PluginManager::pluginLoaded,
          hardwareTree, &HardwareTreeWidget::refreshTree);
  connect(&pluginMgr, &etest::core::plugin::PluginManager::pluginUnloaded,
          hardwareTree, &HardwareTreeWidget::refreshTree);

  // 日志输出到界面
  auto* qtSink = etest::core::logger::Logger::qtConsoleSink();
  if (qtSink) {
    connect(qtSink, &QtConsoleSink::logMessage, output_panel_,
            &OutputPanel::appendLog);
  }
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
  view_aux_sidebar_action_ = view_menu_->addAction(QStringLiteral("辅助侧边栏"));
  view_aux_sidebar_action_->setCheckable(true);
  view_aux_sidebar_action_->setChecked(true);
  connect(view_aux_sidebar_action_, &QAction::triggered, this, [this](bool checked) {
    auto* auxDock = dock_manager_->findDockWidget("AuxSidebarDock");
    if (auxDock) {
      auxDock->toggleView(checked);
      // toggleView会重建标题栏，需要重新隐藏按钮
      hideDockTitleBarButtons(auxDock->dockAreaWidget());
    }
  });

  menuBar->addMenu(QStringLiteral("工具(&T)"));
  menuBar->addMenu(QStringLiteral("帮助(&H)"));
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
  status_message_label_->setText(QStringLiteral("项目已打开：%1").arg(projectPath));
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
        // 先尝试关闭当前项目
        if (!tryCloseCurrentProject()) {
          return;  // 用户取消，不继续打开
        }

        auto& pm = etest::core::project::ProjectManager::instance();
        if (!pm.openProject(path)) {
          QMessageBox::warning(
              this, QStringLiteral("打开项目失败"),
              QStringLiteral("无法打开项目文件：%1").arg(path));
        }
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
  if (!editor->saveFile()) {
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
    if (!editor->saveFileAs(newPath)) {
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
  editor_manager_->closeFile(editor->filePath());
}

void MainWindow::onCloseAllFiles() {
  editor_manager_->closeAllFiles();
}

void MainWindow::onUndo() {
  if (auto* editor = editor_manager_->currentEditor()) {
    editor->editor()->undo();
  }
}

void MainWindow::onRedo() {
  if (auto* editor = editor_manager_->currentEditor()) {
    editor->editor()->redo();
  }
}

void MainWindow::onCut() {
  if (auto* editor = editor_manager_->currentEditor()) {
    editor->editor()->cut();
  }
}

void MainWindow::onCopy() {
  if (auto* editor = editor_manager_->currentEditor()) {
    editor->editor()->copy();
  }
}

void MainWindow::onPaste() {
  if (auto* editor = editor_manager_->currentEditor()) {
    editor->editor()->paste();
  }
}

void MainWindow::onFind() {
  auto* editor = editor_manager_->currentEditor();
  if (!editor)
    return;

  bool ok;
  QString searchText = QInputDialog::getText(this, QStringLiteral("查找"),
                                             QStringLiteral("查找内容:"),
                                             QLineEdit::Normal, QString(), &ok);
  if (ok && !searchText.isEmpty()) {
    // 获取当前光标位置
    int line, column;
    editor->editor()->getCursorPosition(&line, &column);

    // 查找第一个匹配项
    bool found = editor->editor()->findFirst(searchText, false, false, false,
                                             true, true, line, column, true);
    if (!found) {
      QMessageBox::information(this, QStringLiteral("查找"),
                               QStringLiteral("找不到指定内容"));
    }
  }
}

void MainWindow::onReplace() {
  auto* editor = editor_manager_->currentEditor();
  if (!editor)
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

  // 获取当前光标位置
  int line, column;
  editor->editor()->getCursorPosition(&line, &column);

  // 查找第一个匹配项
  bool found = editor->editor()->findFirst(searchText, false, false, false,
                                           true, true, line, column, true);
  if (found) {
    int ret = QMessageBox::question(
        this, QStringLiteral("替换"), QStringLiteral("替换当前匹配项吗？"),
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
    if (ret == QMessageBox::Cancel) {
      return;
    } else if (ret == QMessageBox::Yes) {
      editor->editor()->replace(replaceText);
    }

    // 继续查找下一个
    while (editor->editor()->findNext()) {
      ret = QMessageBox::question(
          this, QStringLiteral("替换"), QStringLiteral("替换当前匹配项吗？"),
          QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
      if (ret == QMessageBox::Cancel) {
        break;
      } else if (ret == QMessageBox::Yes) {
        editor->editor()->replace(replaceText);
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
  if (!editor)
    return;

  int lineCount = editor->editor()->lines();
  bool ok;

  // 获取当前行号
  int currentLine, currentColumn;
  editor->editor()->getCursorPosition(&currentLine, &currentColumn);

  int lineNumber =
      QInputDialog::getInt(this, QStringLiteral("跳转到行"),
                           QStringLiteral("行号 (1-%1):").arg(lineCount),
                           currentLine + 1,  // 当前行号
                           1, lineCount, 1, &ok);
  if (ok) {
    editor->editor()->setCursorPosition(lineNumber - 1, 0);
    editor->editor()->ensureLineVisible(lineNumber - 1);
  }
}

void MainWindow::closeEvent(QCloseEvent* event) {
  // 先尝试关闭所有编辑器文件，如果用户取消则不关闭程序
  if (!editor_manager_->closeAllFiles()) {
    event->ignore();
    return;
  }

  // 再关闭项目
  auto& projectMgr = etest::core::project::ProjectManager::instance();
  if (projectMgr.isProjectOpen()) {
    projectMgr.closeProject();
  }

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

  int w = cfg.get<int>(CONFIG_WINDOW_WIDTH, 1200);
  int h = cfg.get<int>(CONFIG_WINDOW_HEIGHT, 800);
  resize(w, h);

  int x = cfg.get<int>(CONFIG_WINDOW_X, -1);
  int y = cfg.get<int>(CONFIG_WINDOW_Y, -1);
  if (x >= 0 && y >= 0) {
    move(x, y);
  }

  if (cfg.get<bool>(CONFIG_WINDOW_MAXIMIZED, false)) {
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
  auto* activityDock = dock_manager_->findDockWidget("ActivityDock");
  if (activityDock && activityDock->dockAreaWidget()) {
    activityDock->dockAreaWidget()->titleBar()->hide();
  };
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

void MainWindow::hideDockTitleBarButtons(ads::CDockAreaWidget* area) {
  if (!area) return;
  auto* titleBar = area->titleBar();
  if (!titleBar) return;
  for (auto* btn : titleBar->findChildren<QToolButton*>()) {
    auto name = btn->objectName();
    if (name == "tabsMenuButton" || name == "detachGroupButton" ||
        name == "dockAreaCloseButton") {
      btn->hide();
    }
  }
}

}  // namespace etest::app
