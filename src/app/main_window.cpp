#include "main_window.h"

#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include "dialogs/AboutDialog.h"
#include "dialogs/LoginDialog.h"
#include "dialogs/UserManagerDialog.h"
#include <QScrollBar>
#include <QShortcut>
#include <QSplitter>
#include <QStatusBar>
#include <QToolButton>


#include "SARibbonBar.h"
#include "SARibbonCategory.h"
#include "SARibbonPanel.h"
#include "SARibbonQuickAccessBar.h"

#include <DockAreaTitleBar.h>
#include <DockAreaWidget.h>
#include <DockSplitter.h>
#include <DockWidgetTab.h>
#include "ActivityBarWidget.h"
#include "AppIconProvider.h"
#include "BottomContainerWidget.h"
#include "EditorManager.h"
#include "GitWidget.h"
#include "HardwareTreeWidget.h"
#include "OutputPanel.h"
#include "ProblemsPanel.h"
#include "ProjectStructureWidget.h"
#include "ProtocolManagerWidget.h"
#include "SearchWidget.h"
#include "SidebarWidget.h"
#include "TerminalPanel.h"
#include "TestProgramManagerWidget.h"
#include "TextEditorWidget.h"
#include "ThemeManager.h"
#include "WelcomeWidget.h"
#include "api/IEditor.h"
#include "backup/BackupManager.h"
#include "config/ConfigDefs.h"
#include "config/ConfigManager.h"
#include "auth/AuthService.h"
#include "dialogs/NewProjectDialog.h"
#include "dialogs/SettingsDialog.h"
#include "logger/Logger.h"
#include "logger/QtConsoleSink.h"
#include "plugin/PluginManager.h"
#include "project/ProjectManager.h"
#include "topology/TopologyDocument.h"
#include "topology/TopologyEditorWidget.h"


using namespace etest::core::config;
using namespace etest::core::project;
using namespace etest::core::logger;
using namespace etest::core::auth;

namespace etest::app {

MainWindow::MainWindow(QWidget* parent)
    : SARibbonMainWindow(parent),
      dock_manager_(nullptr),
      activity_bar_(nullptr),
      sidebar_(nullptr),
      h_splitter_(nullptr),
      v_splitter_(nullptr),
      editor_manager_(nullptr),
      output_panel_(nullptr),
      problems_panel_(nullptr),
      terminal_panel_(nullptr) {
  initUi();
  initSignals();
  // 加载用户数据（首次运行自动创建 admin 默认用户）
  AuthService::instance();
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

  // 初始化 ThemeManager（加载 QSS、检测暗亮、同步遗留状态）
  ThemeManager::instance();

  setupRibbon();
  createStatusBar();

  // ==================== 中央容器 ====================
  auto* centralContainer = new QWidget(this);
  centralContainer->setObjectName("centralContainer");
  auto* main_layout = new QHBoxLayout(centralContainer);
  main_layout->setContentsMargins(0, 0, 0, 0);
  main_layout->setSpacing(0);

  // ==================== 活动栏 ====================
  activity_bar_ = new ActivityBarWidget(centralContainer);
  main_layout->addWidget(activity_bar_);

  // ==================== 水平分割器 ====================
  h_splitter_ = new QSplitter(Qt::Horizontal, centralContainer);
  h_splitter_->setChildrenCollapsible(true);

  // ===== 侧边栏 =====
  sidebar_ = new SidebarWidget(h_splitter_);
  h_splitter_->addWidget(sidebar_);

  // ── 注册侧边栏页面（按活动栏顺序） ──
  // 项目概览 → 后续替换为 ProjectStructureWidget
  sidebar_->addPage(PageId::kProjectOverview, new ProjectStructureWidget(sidebar_),
                    QStringLiteral("项目概览"));
  // 拓扑 → 占位，待 TopologyManagerWidget 实现
  sidebar_->addPage(PageId::kTopology, new QWidget(sidebar_),
                    QStringLiteral("拓扑"));
  // 硬件树
  sidebar_->addPage(PageId::kHardware, new HardwareTreeWidget(sidebar_),
                    QStringLiteral("硬件"));
  // 协议管理器
  sidebar_->addPage(PageId::kProtocol, new ProtocolManagerWidget(sidebar_),
                    QStringLiteral("协议"));
  // 用例管理器
  sidebar_->addPage(PageId::kTestProgram, new TestProgramManagerWidget(sidebar_),
                    QStringLiteral("用例"));
  // 运行 → 占位，待实现
  sidebar_->addPage(PageId::kRun, new QWidget(sidebar_),
                    QStringLiteral("运行"));
  // 报告 → 占位，待实现
  sidebar_->addPage(PageId::kReport, new QWidget(sidebar_),
                    QStringLiteral("报告"));
  // 搜索
  sidebar_->addPage(PageId::kSearch, new SearchWidget(sidebar_),
                    QStringLiteral("搜索"));
  // Git
  sidebar_->addPage(PageId::kGit, new GitWidget(sidebar_),
                    QStringLiteral("Git"));

  // ── 注册活动栏按钮 ──
  activity_bar_->addPage(PageId::kProjectOverview,
                         QStringLiteral("项目概览"), QStringLiteral("project"));
  activity_bar_->addPage(PageId::kTopology, QStringLiteral("拓扑"),
                         QStringLiteral("topo_tap"));
  activity_bar_->addPage(PageId::kHardware, QStringLiteral("硬件"),
                         QStringLiteral("hardware"));
  activity_bar_->addPage(PageId::kProtocol, QStringLiteral("协议"),
                         QStringLiteral("protocol"));
  activity_bar_->addPage(PageId::kTestProgram, QStringLiteral("用例"),
                         QStringLiteral("testprogram"));
  activity_bar_->addPage(PageId::kRun, QStringLiteral("运行"),
                         QStringLiteral("debug"));
  activity_bar_->addPage(PageId::kReport, QStringLiteral("报告"),
                         QStringLiteral("project"));
  activity_bar_->addPage(PageId::kSearch, QStringLiteral("搜索"),
                         QStringLiteral("search"));
  activity_bar_->addPage(PageId::kGit, QStringLiteral("Git"),
                         QStringLiteral("git"));

  // ===== 垂直分割器（编辑器 + 底部面板） =====
  v_splitter_ = new QSplitter(Qt::Vertical, h_splitter_);
  v_splitter_->setChildrenCollapsible(true);
  h_splitter_->addWidget(v_splitter_);

  // ===== 编辑器区域 =====
  ads::CDockManager::setConfigFlag(ads::CDockManager::AlwaysShowTabs, true);
  ads::CDockManager::setConfigFlag(
      ads::CDockManager::MiddleMouseButtonClosesTab, true);
  dock_manager_ = new ads::CDockManager(v_splitter_);

  // 中央编辑区：Welcome页面
  welcome_widget_ = new WelcomeWidget(this);
  central_dock_ = new ads::CDockWidget(QStringLiteral("欢迎"));
  central_dock_->setObjectName("CentralDock");
  central_dock_->setWidget(welcome_widget_);
  central_dock_->tabWidget()->setElideMode(Qt::ElideNone);
  dock_manager_->setCentralWidget(central_dock_);

  // 允许关闭欢迎页
  central_dock_->setFeature(ads::CDockWidget::DockWidgetClosable, true);

  // 隐藏中央区域标题栏的菜单和分离按钮
  auto* centralArea = central_dock_->dockAreaWidget();
  if (centralArea) {
    hideDockTitleBarButtons(centralArea);
  }

  // 编辑器管理器
  editor_manager_ = new EditorManager(dock_manager_, this);

  // ===== 底部面板 =====
  output_panel_ = new OutputPanel(this);
  problems_panel_ = new ProblemsPanel(this);
  terminal_panel_ = new TerminalPanel(this);

  bottom_container_ = new BottomContainerWidget(v_splitter_);
  bottom_container_->addPanel(QStringLiteral("输出"), output_panel_);
  bottom_container_->addPanel(QStringLiteral("问题"), problems_panel_);
  bottom_container_->addPanel(QStringLiteral("终端"), terminal_panel_);
  v_splitter_->addWidget(bottom_container_);

  // ===== 辅助侧边栏 =====
  aux_sidebar_widget_ = new QWidget(h_splitter_);
  auto* aux_layout = new QVBoxLayout(aux_sidebar_widget_);
  aux_layout->setContentsMargins(0, 0, 0, 0);
  auto* auxLabel =
      new QLabel(QStringLiteral("辅助侧边栏"), aux_sidebar_widget_);
  auxLabel->setAlignment(Qt::AlignCenter);
  aux_layout->addWidget(auxLabel);
  aux_sidebar_widget_->hide();  // 默认隐藏
  h_splitter_->addWidget(aux_sidebar_widget_);

  // 设置 splitter 初始尺寸
  h_splitter_->setSizes({280, 800, 0});  // sidebar / 垂直区域 / aux
  v_splitter_->setSizes({600, 200});     // 编辑器 / 底部面板

  main_layout->addWidget(h_splitter_);
  setCentralWidget(centralContainer);

  // 恢复窗口状态
  restoreWindowState();
}

void MainWindow::onThemeChanged(bool isDark) {
  // 同步设置对话框样式（QSS 已由 ThemeManager 全局加载到 qApp）
  // ADS dock manager 样式跟随全局 QSS，无需单独设置
  if (settings_dialog_) {
    settings_dialog_->setStyleSheet(qApp->styleSheet());
  }
  setRibbonTheme(isDark ? SARibbonTheme::RibbonThemeDark2
                        : SARibbonTheme::RibbonThemeOffice2021Blue);
}

void MainWindow::initSignals() {
  // 主题切换：连接 ThemeManager 信号
  connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this,
          &MainWindow::onThemeChanged);

  // Ribbon 展开/收起状态持久化
  connect(ribbonBar(), &SARibbonBar::ribbonModeChanged, this,
          [](SARibbonBar::RibbonMode mode) {
            ConfigManager::instance().set<bool>(
                CONFIG_RIBBON_MINIMIZED,
                mode == SARibbonBar::MinimumRibbonMode);
          });

  // 视图菜单：输出面板显隐
  connect(view_panel_action_, &QAction::triggered, this, [this](bool checked) {
    if (checked) {
      bottom_container_->show();
      auto sizes = v_splitter_->sizes();
      if (sizes.size() >= 2) {
        sizes[1] = bottom_container_height_;
        v_splitter_->setSizes(sizes);
      }
    } else {
      auto sizes = v_splitter_->sizes();
      if (sizes.size() >= 2) {
        bottom_container_height_ = sizes[1];
      }
      bottom_container_->hide();
    }
  });

  // 视图菜单：辅助侧边栏显隐
  connect(view_aux_sidebar_action_, &QAction::triggered, this,
          [this](bool checked) {
            if (checked) {
              aux_sidebar_widget_->show();
              auto sizes = h_splitter_->sizes();
              if (sizes.size() >= 3) {
                sizes[2] = aux_sidebar_width_;
                h_splitter_->setSizes(sizes);
              }
            } else {
              auto sizes = h_splitter_->sizes();
              if (sizes.size() >= 3) {
                aux_sidebar_width_ = sizes[2];
              }
              aux_sidebar_widget_->hide();
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

  // 项目结构树：项目打开/关闭时切换
  auto* psWidget = qobject_cast<ProjectStructureWidget*>(
      sidebar_->pageById(PageId::kProjectOverview));
  if (psWidget) {
    connect(&projectMgr, &etest::core::project::ProjectManager::projectOpened,
            psWidget, &ProjectStructureWidget::setProjectPath);
    connect(&projectMgr, &etest::core::project::ProjectManager::projectClosed,
            psWidget, &ProjectStructureWidget::clearProjectPath);

    // 项目结构树：双击文件打开编辑器
    connect(psWidget, &ProjectStructureWidget::fileOpenRequested, psWidget,
            [this](const QString& path) { editor_manager_->openFile(path); });
    // 项目结构树：右键→用文本编辑器打开
    connect(psWidget, &ProjectStructureWidget::fileOpenAsTextRequested,
            psWidget, [this](const QString& path) {
              editor_manager_->openFile(path, QStringLiteral("text"));
            });
    // 项目结构树：文件删除/重命名同步到编辑器
    connect(psWidget, &ProjectStructureWidget::fileDeleted, editor_manager_,
            &EditorManager::onFileDeleted);
    connect(psWidget, &ProjectStructureWidget::fileRenamed, editor_manager_,
            &EditorManager::onFileRenamed);
  }

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
  connect(gitWidget, &GitWidget::fileOpenRequested, gitWidget,
          [this](const QString& path) { editor_manager_->openFile(path); });

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
    if (sidebar_->isContentVisible()) {
      auto sizes = h_splitter_->sizes();
      if (!sizes.isEmpty()) {
        sidebar_expanded_width_ = sizes[0];
        sizes[0] = 0;
        h_splitter_->setSizes(sizes);
      }
      sidebar_->hideContent();
    } else {
      sidebar_->showContent();
      auto sizes = h_splitter_->sizes();
      if (!sizes.isEmpty()) {
        sizes[0] = sidebar_expanded_width_;
        h_splitter_->setSizes(sizes);
      }
    }
  });

  // Ctrl+Shift+F 全局搜索
  auto* globalSearchShortcut =
      new QShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_F), this);
  connect(globalSearchShortcut, &QShortcut::activated, this, [this]() {
    if (!sidebar_->isContentVisible()) {
      sidebar_->showContent();
      auto sizes = h_splitter_->sizes();
      if (!sizes.isEmpty()) {
        sizes[0] = sidebar_expanded_width_;
        h_splitter_->setSizes(sizes);
      }
    }
    sidebar_->switchPage(PageId::kSearch);
    activity_bar_->setActivePageId(PageId::kSearch);
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
  connect(protocolMgr, &ProtocolManagerWidget::openFileRequested, protocolMgr,
          [this](const QString& path) { editor_manager_->openFile(path); });

  // 协议管理器：项目打开/关闭时刷新
  connect(&projectMgr, &etest::core::project::ProjectManager::projectOpened,
          protocolMgr, &ProtocolManagerWidget::refreshList);
  connect(&projectMgr, &etest::core::project::ProjectManager::projectClosed,
          protocolMgr, &ProtocolManagerWidget::refreshList);

  // 用例管理器：双击文件打开编辑器
  auto* tpMgr = sidebar_->testProgramManager();
  connect(tpMgr, &TestProgramManagerWidget::openFileRequested, tpMgr,
          [this](const QString& path) { editor_manager_->openFile(path); });

  // 用例管理器：项目打开/关闭时刷新
  connect(&projectMgr, &etest::core::project::ProjectManager::projectOpened,
          tpMgr, &TestProgramManagerWidget::refreshList);
  connect(&projectMgr, &etest::core::project::ProjectManager::projectClosed,
          tpMgr, &TestProgramManagerWidget::refreshList);

  // 日志输出到界面
  auto* qtSink = etest::core::logger::Logger::qtConsoleSink();
  if (qtSink) {
    connect(qtSink, &QtConsoleSink::logMessage, output_panel_,
            &OutputPanel::appendLog);
  }

  // 活动栏：设置对话框
  connect(activity_bar_, &ActivityBarWidget::settingsTriggered, this, [this]() {
    if (!settings_dialog_) {
      settings_dialog_ = new SettingsDialog(this);
      // QDialog 作为独立窗口，需要主动继承 MainWindow 的样式表
      settings_dialog_->setStyleSheet(styleSheet());
    }
    settings_dialog_->show();
    settings_dialog_->raise();
    settings_dialog_->activateWindow();
  });

  // 活动栏：页面切换
  connect(activity_bar_, &ActivityBarWidget::pageClicked, this,
          [this](const QString& id) {
            bool samePage = (id == activity_bar_->activePageId());

            if (samePage && sidebar_->isContentVisible()) {
              // 再次点击同一按钮，隐藏侧边栏
              auto sizes = h_splitter_->sizes();
              if (!sizes.isEmpty()) {
                sidebar_expanded_width_ = sizes[0];
                sizes[0] = 0;
                h_splitter_->setSizes(sizes);
              }
              sidebar_->hideContent();
              return;
            }

            // 确保侧边栏可见
            if (!sidebar_->isContentVisible()) {
              sidebar_->showContent();
              auto sizes = h_splitter_->sizes();
              if (!sizes.isEmpty()) {
                sizes[0] = sidebar_expanded_width_;
                h_splitter_->setSizes(sizes);
              }
            }

            sidebar_->switchPage(id);
            activity_bar_->setActivePageId(id);
          });

  // 底部面板关闭按钮
  connect(bottom_container_, &BottomContainerWidget::panelClosed, this,
          [this]() {
            auto sizes = v_splitter_->sizes();
            if (sizes.size() >= 2) {
              bottom_container_height_ = sizes[1];
            }
            bottom_container_->hide();
            if (view_panel_action_) {
              view_panel_action_->setChecked(false);
            }
          });

  // 登录认证
  connect(login_action_, &QAction::triggered, this, [this]() {
    if (AuthService::instance().isLoggedIn()) {
      login_menu_->exec(QCursor::pos());
    } else {
      auto* dlg = new LoginDialog(this);
      connect(dlg, &QDialog::finished, dlg, &QObject::deleteLater);
      dlg->show();
    }
  });

  connect(&AuthService::instance(), &AuthService::loginSucceeded,
          this, [this](const User& user) {
    login_action_->setIcon(
        AppIconProvider::instance().icon(QStringLiteral("account")));
    login_action_->setToolTip(
        QStringLiteral("当前用户：%1 (%2)")
            .arg(user.userName)
            .arg(user.role == UserRole::Admin
                     ? QStringLiteral("Admin")
                     : QStringLiteral("User")));
    login_action_->setText(
        QStringLiteral("%1 (%2)")
            .arg(user.userName)
            .arg(user.role == UserRole::Admin
                     ? QStringLiteral("Admin")
                     : QStringLiteral("User")));

    login_user_info_action_->setText(
        QStringLiteral("%1 (%2)")
            .arg(user.userName)
            .arg(user.role == UserRole::Admin
                     ? QStringLiteral("Admin")
                     : QStringLiteral("User")));
    login_manage_users_action_->setVisible(
        user.role == UserRole::Admin);
  });

  connect(&AuthService::instance(), &AuthService::loggedOut,
          this, [this]() {
    login_action_->setIcon(
        AppIconProvider::instance().icon(QStringLiteral("account")));
    login_action_->setToolTip(QStringLiteral("登录"));
    login_action_->setText(QStringLiteral("登录"));
    login_action_->setMenu(nullptr);
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
  QString msg = fi.exists() ? QStringLiteral("无法打开项目文件：%1").arg(path)
                            : QStringLiteral(
                                  "项目文件 \"%1\" "
                                  "不存在，\n文件可能已被移动或删除。\n\n是否从"
                                  "最近项目中移除此记录？")
                                  .arg(path);

  auto buttons =
      fi.exists() ? QMessageBox::Ok : (QMessageBox::Yes | QMessageBox::No);

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
      recent_projects_menu_->addAction(
          path, this, [this, path]() { openRecentProject(path); });
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
    } else {
      editor_manager_->updateEditorId(editor, newPath);
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
    int ret = QMessageBox::Yes;
    while (textEditor->editor()->findNext()) {
      QMessageBox msgBox(this);
      msgBox.setText(QStringLiteral("替换"));
      msgBox.setInformativeText(QStringLiteral("替换当前匹配项吗？"));
      auto* yesAllBtn =
          msgBox.addButton(QStringLiteral("全部替换"), QMessageBox::YesRole);
      msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No |
                                QMessageBox::Cancel);
      msgBox.setDefaultButton(QMessageBox::Yes);

      ret = msgBox.exec();
      if (ret == QMessageBox::Cancel) {
        break;
      }
      if (ret == QMessageBox::Yes || msgBox.clickedButton() == yesAllBtn) {
        textEditor->editor()->replace(replaceText);
      }
      if (msgBox.clickedButton() == yesAllBtn) {
        while (textEditor->editor()->findNext()) {
          textEditor->editor()->replaceSelectedText(replaceText);
        }
        break;
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
  // 尝试关闭所有编辑器文件，如果用户取消则不关闭程序
  if (!editor_manager_->closeAllFiles()) {
    event->ignore();
    return;
  }

  // 关闭项目
  auto& projectMgr = etest::core::project::ProjectManager::instance();
  if (projectMgr.isProjectOpen()) {
    projectMgr.closeProject();
  }

  saveWindowState();
  QMainWindow::closeEvent(event);
}

//-----------------------------------------------------------------------------
// Ribbon
//-----------------------------------------------------------------------------

void MainWindow::setupRibbon() {
  auto* ribbon = ribbonBar();

  // ---- 创建文件动作（原 createMenuBar 中的逻辑） ----
  new_project_action_ = new QAction(QStringLiteral("新建项目"), this);
  new_project_action_->setShortcut(QStringLiteral("Ctrl+Shift+N"));
  connect(new_project_action_, &QAction::triggered, this,
          &MainWindow::onNewProject);

  open_project_action_ = new QAction(QStringLiteral("打开项目"), this);
  open_project_action_->setShortcut(QKeySequence::Open);
  connect(open_project_action_, &QAction::triggered, this,
          &MainWindow::onOpenProject);

  close_project_action_ = new QAction(QStringLiteral("关闭项目"), this);
  close_project_action_->setEnabled(false);
  connect(close_project_action_, &QAction::triggered, this,
          &MainWindow::onCloseProject);

  save_action_ = new QAction(QStringLiteral("保存"), this);
  save_action_->setShortcut(QKeySequence::Save);
  save_action_->setShortcutContext(Qt::ApplicationShortcut);
  save_action_->setEnabled(false);
  connect(save_action_, &QAction::triggered, this, &MainWindow::onSaveFile);

  save_as_action_ = new QAction(QStringLiteral("另存为..."), this);
  save_as_action_->setEnabled(false);
  connect(save_as_action_, &QAction::triggered, this,
          &MainWindow::onSaveFileAs);

  save_all_action_ = new QAction(QStringLiteral("保存所有"), this);
  save_all_action_->setShortcut(QStringLiteral("Ctrl+Shift+S"));
  save_all_action_->setEnabled(false);
  connect(save_all_action_, &QAction::triggered, this,
          &MainWindow::onSaveAllFiles);

  close_file_action_ = new QAction(QStringLiteral("关闭文件"), this);
  close_file_action_->setShortcut(QKeySequence::Close);
  close_file_action_->setEnabled(false);
  connect(close_file_action_, &QAction::triggered, this,
          &MainWindow::onCloseCurrentFile);

  close_all_files_action_ = new QAction(QStringLiteral("关闭所有文件"), this);
  close_all_files_action_->setEnabled(false);
  connect(close_all_files_action_, &QAction::triggered, this,
          &MainWindow::onCloseAllFiles);

  // ---- 创建编辑动作（原 createEditMenu 中的逻辑） ----
  edit_undo_action_ = new QAction(style()->standardIcon(QStyle::SP_ArrowBack),
                                  QStringLiteral("撤销"), this);
  edit_undo_action_->setShortcut(QKeySequence::Undo);
  edit_undo_action_->setEnabled(false);

  edit_redo_action_ =
      new QAction(style()->standardIcon(QStyle::SP_ArrowForward),
                  QStringLiteral("重做"), this);
  edit_redo_action_->setShortcut(QKeySequence::Redo);
  edit_redo_action_->setEnabled(false);

  edit_cut_action_ = new QAction(QStringLiteral("剪切"), this);
  edit_cut_action_->setShortcut(QKeySequence::Cut);
  edit_cut_action_->setEnabled(false);

  edit_copy_action_ = new QAction(QStringLiteral("复制"), this);
  edit_copy_action_->setShortcut(QKeySequence::Copy);
  edit_copy_action_->setEnabled(false);

  edit_paste_action_ = new QAction(QStringLiteral("粘贴"), this);
  edit_paste_action_->setShortcut(QKeySequence::Paste);
  edit_paste_action_->setEnabled(false);

  edit_find_action_ = new QAction(QStringLiteral("查找"), this);
  edit_find_action_->setShortcut(QKeySequence::Find);
  edit_find_action_->setEnabled(false);

  edit_replace_action_ = new QAction(QStringLiteral("替换"), this);
  edit_replace_action_->setShortcut(QKeySequence::Replace);
  edit_replace_action_->setEnabled(false);

  edit_go_to_line_action_ = new QAction(QStringLiteral("跳转到行"), this);
  edit_go_to_line_action_->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_G));
  edit_go_to_line_action_->setEnabled(false);

  // ---- QuickAccessBar ----
  auto* qab = ribbon->quickAccessBar();
  new_project_action_->setIcon(style()->standardIcon(QStyle::SP_FileIcon));
  qab->addAction(new_project_action_);
  open_project_action_->setIcon(
      style()->standardIcon(QStyle::SP_DialogOpenButton));
  qab->addAction(open_project_action_);
  save_action_->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
  qab->addAction(save_action_);
  qab->addSeparator();

  edit_undo_action_->setIcon(style()->standardIcon(QStyle::SP_ArrowBack));
  qab->addAction(edit_undo_action_);
  edit_redo_action_->setIcon(style()->standardIcon(QStyle::SP_ArrowForward));
  qab->addAction(edit_redo_action_);

  // ── 登录按钮 ──
  login_action_ = new QAction(
      AppIconProvider::instance().icon(QStringLiteral("account")),
      QStringLiteral("登录"), this);
  login_action_->setToolTip(QStringLiteral("登录"));
  qab->addAction(login_action_);

  // 登录后的菜单
  login_menu_ = new QMenu(this);
  login_user_info_action_ = login_menu_->addAction(QStringLiteral("admin (Admin)"));
  login_user_info_action_->setEnabled(false);
  login_menu_->addSeparator();
  login_manage_users_action_ = login_menu_->addAction(
      QStringLiteral("用户管理"));
  connect(login_manage_users_action_, &QAction::triggered, this, [this]() {
    UserManagerDialog dlg(this);
    dlg.exec();
  });
  login_menu_->addSeparator();
  auto* logoutAction = login_menu_->addAction(QStringLiteral("退出登录"));
  connect(logoutAction, &QAction::triggered, this, [this]() {
    AuthService::instance().logout();
  });

  // ---- Application Button ----
  ribbon->applicationButton()->setIcon(
      style()->standardIcon(QStyle::SP_FileIcon));
  ribbon->applicationButton()->setText(QStringLiteral("文件"));

  // 文件 Application Button 菜单 (QMenu)
  auto* app_menu = new QMenu(this);
  app_menu->addAction(close_project_action_);
  app_menu->addSeparator();
  app_menu->addAction(save_all_action_);
  app_menu->addAction(save_as_action_);
  app_menu->addSeparator();
  app_menu->addAction(close_file_action_);
  app_menu->addAction(close_all_files_action_);
  app_menu->addSeparator();

  recent_projects_menu_ = app_menu->addMenu(QStringLiteral("最近项目"));
  updateRecentProjectsMenu();

  app_menu->addSeparator();
  app_menu->addAction(QStringLiteral("退出"), this, &QWidget::close);

  // 将 QMenu 挂到 Application Button 上
  if (auto* app_btn = qobject_cast<QToolButton*>(ribbon->applicationButton())) {
    app_btn->setMenu(app_menu);
  }

  // ============================================================
  //  主页
  // ============================================================
  {
    auto* cat = ribbon->addCategoryPage(QStringLiteral("主页"));

    // 文件 Panel
    auto* panel_file = cat->addPanel(QStringLiteral("文件"));
    panel_file->addLargeAction(new_project_action_);
    panel_file->addLargeAction(open_project_action_);
    panel_file->addLargeAction(save_action_);

    // 编辑 Panel
    auto* panel_edit = cat->addPanel(QStringLiteral("编辑"));
    panel_edit->addLargeAction(edit_undo_action_);
    panel_edit->addLargeAction(edit_redo_action_);
    panel_edit->addSeparator();

    edit_cut_action_->setIcon(style()->standardIcon(QStyle::SP_FileLinkIcon));
    edit_copy_action_->setIcon(style()->standardIcon(QStyle::SP_FileIcon));
    edit_paste_action_->setIcon(style()->standardIcon(QStyle::SP_FileLinkIcon));
    panel_edit->addSmallAction(edit_cut_action_);
    panel_edit->addSmallAction(edit_copy_action_);
    panel_edit->addSmallAction(edit_paste_action_);

    panel_edit->addSeparator();
    panel_edit->addSmallAction(edit_find_action_);
    panel_edit->addSmallAction(edit_replace_action_);
    panel_edit->addSmallAction(edit_go_to_line_action_);
  }

  // ============================================================
  //  视图
  // ============================================================
  {
    auto* cat = ribbon->addCategoryPage(QStringLiteral("视图"));

    auto* panel_panels = cat->addPanel(QStringLiteral("面板"));

    auto* act_welcome = new QAction(QStringLiteral("欢迎页"), this);
    connect(act_welcome, &QAction::triggered, this, [this]() {
      auto* centralDock = dock_manager_->findDockWidget("CentralDock");
      if (!centralDock)
        return;
      if (centralDock->isClosed())
        centralDock->toggleView(true);
      if (auto* area = centralDock->dockAreaWidget())
        area->setCurrentIndex(0);
    });
    panel_panels->addLargeAction(act_welcome);

    view_panel_action_ = new QAction(QStringLiteral("输出面板"), this);
    view_panel_action_->setCheckable(true);
    view_panel_action_->setChecked(true);
    view_panel_action_->setShortcut(QStringLiteral("Ctrl+J"));
    panel_panels->addLargeAction(view_panel_action_);

    view_aux_sidebar_action_ = new QAction(QStringLiteral("辅助侧边栏"), this);
    view_aux_sidebar_action_->setCheckable(true);
    view_aux_sidebar_action_->setChecked(false);
    panel_panels->addLargeAction(view_aux_sidebar_action_);
  }

  // ============================================================
  //  工具
  // ============================================================
  {
    auto* cat = ribbon->addCategoryPage(QStringLiteral("工具"));

    auto* panel_tools = cat->addPanel(QStringLiteral("工具"));
    auto* act_settings =
        new QAction(style()->standardIcon(QStyle::SP_FileDialogNewFolder),
                    QStringLiteral("设置"), this);
    connect(act_settings, &QAction::triggered, this, [this]() {
      if (!settings_dialog_) {
        settings_dialog_ = new SettingsDialog(this);
        settings_dialog_->setStyleSheet(styleSheet());
      }
      settings_dialog_->show();
      settings_dialog_->raise();
      settings_dialog_->activateWindow();
    });
    panel_tools->addLargeAction(act_settings);
  }

  // ============================================================
  //  帮助
  // ============================================================
  {
    auto* cat = ribbon->addCategoryPage(QStringLiteral("帮助"));

    auto* panel_about = cat->addPanel(QStringLiteral("关于"));
    auto* act_about = new QAction(QStringLiteral("关于 ETest Demo"), this);
    connect(act_about, &QAction::triggered, this, [this]() {
      AboutDialog dlg(this);
      dlg.exec();
    });
    panel_about->addLargeAction(act_about);
  }

  // Ribbon style & collapse
  ribbon->setRibbonStyle(SARibbonBar::RibbonStyleLooseThreeRow);
  ribbon->showMinimumModeButton(true);
  ribbon->setTabDoubleClickToMinimumMode(true);

  // 恢复已保存的折叠状态
  bool minimized = ConfigManager::instance().get<bool>(
      CONFIG_RIBBON_MINIMIZED, CONFIG_RIBBON_DEFAULT_MINIMIZED);
  ribbon->setMinimumMode(minimized);

  // 初始化 Ribbon 主题，与当前 ThemeManager 主题一致
  bool isDark = ThemeManager::instance().isDarkTheme();
  setRibbonTheme(isDark ? SARibbonTheme::RibbonThemeDark2
                        : SARibbonTheme::RibbonThemeOffice2021Blue);
}

void MainWindow::saveWindowState() {
  auto& cfg = ConfigManager::instance();
  cfg.set(CONFIG_WINDOW_WIDTH, width());
  cfg.set(CONFIG_WINDOW_HEIGHT, height());
  cfg.set(CONFIG_WINDOW_X, x());
  cfg.set(CONFIG_WINDOW_Y, y());
  cfg.set(CONFIG_WINDOW_MAXIMIZED, isMaximized());

  // Splitter 状态
  cfg.set(CONFIG_WINDOW_H_SPLITTER_STATE, h_splitter_->saveState());
  cfg.set(CONFIG_WINDOW_V_SPLITTER_STATE, v_splitter_->saveState());

  // 侧边栏状态
  bool sidebarVis = sidebar_->isContentVisible();
  cfg.set(CONFIG_SIDEBAR_VISIBLE, sidebarVis);
  if (sidebarVis) {
    auto sizes = h_splitter_->sizes();
    if (!sizes.isEmpty()) {
      sidebar_expanded_width_ = sizes[0];
    }
  }
  cfg.set(CONFIG_SIDEBAR_EXPANDED_WIDTH, sidebar_expanded_width_);
  cfg.set(CONFIG_SIDEBAR_ACTIVE_PAGE, activity_bar_->activePageId());

  // 底部面板状态
  cfg.set(CONFIG_BOTTOM_PANEL_HEIGHT, bottom_container_height_);
  cfg.set(CONFIG_BOTTOM_PANEL_VISIBLE, bottom_container_->isVisible());

  // 辅助侧边栏状态
  cfg.set(CONFIG_AUX_SIDEBAR_WIDTH, aux_sidebar_width_);
  cfg.set(CONFIG_AUX_SIDEBAR_VISIBLE, aux_sidebar_widget_->isVisible());
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

  // Splitter 状态
  QByteArray hState = cfg.get<QByteArray>(CONFIG_WINDOW_H_SPLITTER_STATE);
  if (!hState.isEmpty()) {
    h_splitter_->restoreState(hState);
  }
  QByteArray vState = cfg.get<QByteArray>(CONFIG_WINDOW_V_SPLITTER_STATE);
  if (!vState.isEmpty()) {
    v_splitter_->restoreState(vState);
  }

  // 侧边栏可见性
  sidebar_expanded_width_ = cfg.get<int>(CONFIG_SIDEBAR_EXPANDED_WIDTH, 280);
  bool sidebarVisible = cfg.get<bool>(CONFIG_SIDEBAR_VISIBLE, true);
  if (sidebarVisible) {
    sidebar_->showContent();
  } else {
    sidebar_->hideContent();
    auto sizes = h_splitter_->sizes();
    if (!sizes.isEmpty() && sizes[0] > 0) {
      sizes[0] = 0;
      h_splitter_->setSizes(sizes);
    }
  }

  // 侧边栏活动页面
  QString activePage = cfg.get<QString>(CONFIG_SIDEBAR_ACTIVE_PAGE, PageId::kProjectOverview);
  if (sidebar_->pageById(activePage)) {
    sidebar_->switchPage(activePage);
    activity_bar_->setActivePageId(activePage);
  }

  // 底部面板
  bottom_container_height_ = cfg.get<int>(CONFIG_BOTTOM_PANEL_HEIGHT, 200);
  bool bottomVisible = cfg.get<bool>(CONFIG_BOTTOM_PANEL_VISIBLE, true);
  if (bottomVisible) {
    bottom_container_->show();
    auto sizes = v_splitter_->sizes();
    if (sizes.size() >= 2 && sizes[1] <= 0) {
      sizes[1] = bottom_container_height_;
      v_splitter_->setSizes(sizes);
    }
  } else {
    bottom_container_->hide();
  }
  if (view_panel_action_) {
    view_panel_action_->setChecked(bottomVisible);
  }

  // 辅助侧边栏
  aux_sidebar_width_ = cfg.get<int>(CONFIG_AUX_SIDEBAR_WIDTH, 280);
  bool auxVisible = cfg.get<bool>(CONFIG_AUX_SIDEBAR_VISIBLE, false);
  if (auxVisible) {
    aux_sidebar_widget_->show();
    auto sizes = h_splitter_->sizes();
    if (sizes.size() >= 3 && sizes[2] <= 0) {
      sizes[2] = aux_sidebar_width_;
      h_splitter_->setSizes(sizes);
    }
  } else {
    aux_sidebar_widget_->hide();
  }
  if (view_aux_sidebar_action_) {
    view_aux_sidebar_action_->setChecked(auxVisible);
  }
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