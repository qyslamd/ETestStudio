#include "MainWindow.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QMenu>
#include <QMessageBox>
#include <QProcess>
#include <QScrollBar>
#include <QShortcut>
#include <QSplitter>
#include <QStatusBar>
#include <QTimer>
#include <QToolButton>
#include "dialogs/AboutDialog.h"
#include "dialogs/IcdSignalSelection.h"
#include "dialogs/LoginDialog.h"
#include "dialogs/UserManagerDialog.h"

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
#include "EditorManager.h"
#include "ExecutionDebugWidget.h"
#include "GitWidget.h"
#include "HardwareTreeWidget.h"
#include "ProjectStructureWidget.h"
#include "ProtocolManagerWidget.h"
#include "SearchWidget.h"
#include "SidebarWidget.h"
#include "SignalRegistry.h"
#include "TerminalPanel.h"
#include "TestProgramEditorWidget.h"
#include "TestProgramManagerWidget.h"
#include "TestProgramData.h"
#include "ThemeManager.h"
#include "TopologyManagerWidget.h"
#include "WelcomeWidget.h"
#include "api/IEditor.h"
#include "auth/AuthService.h"
#include "backup/BackupManager.h"
#include "config/ConfigDefs.h"
#include "config/ConfigManager.h"
#include "dialogs/NewProjectDialog.h"
#include "dialogs/SettingsDialog.h"
#include "editors/EditorFactory.h"
#include "editors/TextEditorWidget.h"
#include "engine/StepRunner.h"
#include "engine/TestExecutionEngine.h"
#include "icd/repository.hpp"
#include "logger/Logger.h"
#include "logger/QtConsoleSink.h"
#include "plugin_sdk/PluginManager.h"
#include "project/ProjectManager.h"
#include "protocol/ProtocolEditorWidget.h"
#include "topology/TopologyDocument.h"
#include "topology/TopologyEditorWidget.h"
#include "utils/SignalSyncHelper.h"
#include "widgets/BottomContainerWidget.h"
#include "widgets/HintBarWidget.h"
#include "widgets/LoadingOverlay.h"
#include "widgets/ExecutionOutputPanel.h"
#include "widgets/LogOutputPanel.h"
#include "widgets/ProblemsPanel.h"
#include "widgets/TuxSaverOverlay.h"

using namespace etest::core::config;
using namespace etest::core::project;
using namespace etest::core::logger;
using namespace etest::core::auth;

namespace etest::app {

using etest::core_ui::AppIconProvider;
using etest::core_ui::ThemeManager;

MainWindow::MainWindow(QWidget* parent)
    : SARibbonMainWindow(parent),
      dock_manager_(nullptr),
      activity_bar_(nullptr),
      sidebar_(nullptr),
      h_splitter_(nullptr),
      v_splitter_(nullptr),
      editor_manager_(nullptr),
      log_panel_(nullptr),
      execution_output_panel_(nullptr),
      problems_panel_(nullptr),
      terminal_panel_(nullptr),
      signal_registry_(nullptr),
      icd_repository_(nullptr) {
  QElapsedTimer timer;
  timer.start();
  initUi();
  initSignalsEarly();
  // 安排懒加载（窗口 show() 之后执行）
  QTimer::singleShot(0, this, &MainWindow::lazyInit);

  LOG_INFO("MAIN", "主窗口初始化完成");
}

bool MainWindow::eventFilter(QObject* obj, QEvent* event) {
  switch (event->type()) {
    case QEvent::MouseMove:
    case QEvent::MouseButtonPress:
    case QEvent::KeyPress:
    case QEvent::Wheel:
      tux_idle_timer_.restart();
      break;
    default:
      break;
  }
  return false;
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
  setWindowTitle(QStringLiteral("ETestStudio"));
  setMinimumSize(900, 600);
  setWindowIcon(QIcon(":/resources/icons/app_icon.ico"));

  // 初始化 ThemeManager（加载 QSS、检测暗亮、同步遗留状态）
  ThemeManager::instance();

  setupRibbon();
  createStatusBar();

  // ==================== 中央容器 ====================
  auto* centralContainer = new QWidget(this);
  centralContainer->setObjectName("centralContainer");
  auto* main_layout = new QVBoxLayout(centralContainer);
  main_layout->setContentsMargins(0, 0, 0, 0);
  main_layout->setSpacing(0);

  // ===== 提示栏（全宽，顶部） =====
  hint_bar_ = new HintBarWidget(centralContainer);
  main_layout->addWidget(hint_bar_);

  // ==================== 水平布局（活动栏 + 水平分割器） ====================
  auto* horizontal_layout = new QHBoxLayout;
  horizontal_layout->setContentsMargins(0, 0, 0, 0);
  horizontal_layout->setSpacing(0);

  // ==================== 活动栏 ====================
  activity_bar_ = new ActivityBarWidget(centralContainer);
  horizontal_layout->addWidget(activity_bar_);

  // ==================== 水平分割器 ====================
  h_splitter_ = new QSplitter(Qt::Horizontal, centralContainer);
  h_splitter_->setChildrenCollapsible(true);
  horizontal_layout->addWidget(h_splitter_, 1);

  // ===== 侧边栏 =====
  sidebar_ = new SidebarWidget(h_splitter_);
  h_splitter_->addWidget(sidebar_);

  // ===== 垂直分割器（编辑器 + 底部面板） =====
  v_splitter_ = new QSplitter(Qt::Vertical, h_splitter_);
  v_splitter_->setChildrenCollapsible(true);
  h_splitter_->addWidget(v_splitter_);

  ads::CDockManager::setConfigFlag(ads::CDockManager::AlwaysShowTabs, true);
  ads::CDockManager::setConfigFlag(
      ads::CDockManager::MiddleMouseButtonClosesTab, true);
  ads::CDockManager::setConfigFlag(ads::CDockManager::AllTabsHaveCloseButton,
                                   true);
  dock_manager_ = new ads::CDockManager(v_splitter_);

  // 中央占位（lazyInit 时替换为 WelcomeWidget）
  auto* placeholder = new QWidget(v_splitter_);
  placeholder->setObjectName("CentralPlaceholder");
  central_dock_ = new ads::CDockWidget(QStringLiteral("欢迎"));
  central_dock_->setObjectName("CentralDock");
  central_dock_->setWidget(placeholder);
  central_dock_->tabWidget()->setElideMode(Qt::ElideNone);
  dock_manager_->setCentralWidget(central_dock_);
  central_dock_->setFeature(ads::CDockWidget::DockWidgetClosable, true);
  auto* centralArea = central_dock_->dockAreaWidget();
  if (centralArea) {
    hideDockTitleBarButtons(centralArea);
  }

  // 底部容器空壳（lazyInit 时 addPanel）
  bottom_container_ = new BottomContainerWidget(v_splitter_);
  v_splitter_->addWidget(bottom_container_);
  bottom_container_->hide();  // 无面板时隐藏

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
  v_splitter_->setSizes({800, 0});       // 底部面板初始大小为 0（后续恢复）

  main_layout->addLayout(horizontal_layout, 1);
  setCentralWidget(centralContainer);

  // 恢复窗口状态
  restoreWindowState();
}

void MainWindow::initSignalsEarly() {
  // 主题切换
  connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this,
          &MainWindow::onThemeChanged);

  // Ribbon 展开/收起状态持久化
  connect(ribbonBar(), &SARibbonBar::ribbonModeChanged, this,
          [](SARibbonBar::RibbonMode mode) {
            ConfigManager::instance().set<bool>(
                CONFIG_RIBBON_MINIMIZED,
                mode == SARibbonBar::MinimumRibbonMode);
          });

  // 活动栏：设置对话框
  connect(activity_bar_, &ActivityBarWidget::settingsTriggered, this, [this]() {
    if (!settings_dialog_) {
      settings_dialog_ = new SettingsDialog(this);
      settings_dialog_->setStyleSheet(qApp->styleSheet());
      connect(settings_dialog_, &QDialog::finished, this,
              [this]() { activity_bar_->setSettingsActive(false); });
    }
    activity_bar_->setSettingsActive(true);
    settings_dialog_->show();
    settings_dialog_->raise();
    settings_dialog_->activateWindow();
  });

  // 活动栏：页面切换
  connect(activity_bar_, &ActivityBarWidget::pageClicked, this,
          [this](const QString& id) {
            bool samePage = (id == activity_bar_->activePageId());
            if (samePage && sidebar_->isContentVisible()) {
              auto sizes = h_splitter_->sizes();
              if (!sizes.isEmpty()) {
                sidebar_expanded_width_ = sizes[0];
                sizes[0] = 0;
                h_splitter_->setSizes(sizes);
              }
              sidebar_->hideContent();
              activity_bar_->clearActivePage();
              return;
            }
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

  // 活动栏：登录触发
  connect(activity_bar_, &ActivityBarWidget::loginTriggered, this, [this]() {
    if (AuthService::instance().isLoggedIn()) {
      login_menu_->exec(QCursor::pos());
    } else {
      auto* dlg = new LoginDialog(this);
      connect(dlg, &QDialog::finished, this,
              [this]() { activity_bar_->setLoginActive(false); });
      connect(dlg, &QDialog::finished, dlg, &QObject::deleteLater);
      dlg->show();
      activity_bar_->setLoginActive(true);
    }
  });
}

void MainWindow::initSignalsLate() {
  // 辅助 lambda：根据面板显隐状态更新容器显隐
  auto updateContainerVisibility = [this]() {
    bool anyVisible = false;
    for (int i = 0; i < bottom_container_->count(); ++i) {
      if (bottom_container_->isPanelVisible(i)) {
        anyVisible = true;
        break;
      }
    }
    if (anyVisible) {
      bottom_container_->show();
      auto sizes = v_splitter_->sizes();
      if (sizes.size() >= 2 && sizes[1] <= 0) {
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
  };

  // 视图菜单：逐面板显隐
  connect(view_output_action_, &QAction::triggered, this,
          [this, updateContainerVisibility](bool checked) {
            LOG_INFO("MAIN_UI", "切换「日志」面板 [visible={}]", checked);
            int idx = bottom_container_->indexOf(log_panel_);
            if (idx >= 0)
              bottom_container_->setPanelVisible(idx, checked);
            updateContainerVisibility();
          });
  connect(view_execution_output_action_, &QAction::triggered, this,
          [this, updateContainerVisibility](bool checked) {
            LOG_INFO("MAIN_UI", "切换「执行输出」面板 [visible={}]", checked);
            int idx = bottom_container_->indexOf(execution_output_panel_);
            if (idx >= 0)
              bottom_container_->setPanelVisible(idx, checked);
            updateContainerVisibility();
          });
  connect(view_problems_action_, &QAction::triggered, this,
          [this, updateContainerVisibility](bool checked) {
            LOG_INFO("MAIN_UI", "切换「问题」面板 [visible={}]", checked);
            int idx = bottom_container_->indexOf(problems_panel_);
            if (idx >= 0)
              bottom_container_->setPanelVisible(idx, checked);
            updateContainerVisibility();
          });
  connect(view_terminal_action_, &QAction::triggered, this,
          [this, updateContainerVisibility](bool checked) {
            LOG_INFO("MAIN_UI", "切换「终端」面板 [visible={}]", checked);
            int idx = bottom_container_->indexOf(terminal_panel_);
            if (idx >= 0)
              bottom_container_->setPanelVisible(idx, checked);
            updateContainerVisibility();
          });

  // 视图菜单：辅助侧边栏显隐
  connect(view_aux_sidebar_action_, &QAction::triggered, this,
          [this](bool checked) {
            LOG_INFO("MAIN_UI", "切换「辅助侧边栏」 [visible={}]", checked);
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
                sizes[2] = 0;
                h_splitter_->setSizes(sizes);
              }
              aux_sidebar_widget_->hide();
            }
          });

  // 项目管理信号
  auto& projectMgr = etest::core::project::ProjectManager::instance();
  connect(&projectMgr, &etest::core::project::ProjectManager::projectCreated,
          this, &MainWindow::onProjectOpened);
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
    connect(
        psWidget, &ProjectStructureWidget::fileOpenRequested, psWidget,
        [this](const QString& path) {
          // ICDConfig 是配置容器，切到 sidebar 协议页，不打开编辑器
          if (path.contains(QStringLiteral("ICDConfig"), Qt::CaseInsensitive)) {
            if (!sidebar_->isContentVisible()) {
              sidebar_->showContent();
              auto sizes = h_splitter_->sizes();
              if (!sizes.isEmpty()) {
                sizes[0] = sidebar_expanded_width_;
                h_splitter_->setSizes(sizes);
              }
            }
            sidebar_->switchPage(PageId::kProtocol);
            activity_bar_->setActivePageId(PageId::kProtocol);
            return;
          }
          editor_manager_->openFile(path);
        });
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

    // 占位页：项目操作
    connect(psWidget, &ProjectStructureWidget::newProjectRequested, this,
            &MainWindow::onNewProject);
    connect(psWidget, &ProjectStructureWidget::openProjectRequested, this,
            &MainWindow::onOpenProject);
    connect(psWidget, &ProjectStructureWidget::projectOpenRequested, this,
            &MainWindow::openRecentProject);
    // 最近项目变更时刷新占位页
    connect(&projectMgr,
            &etest::core::project::ProjectManager::recentProjectsChanged,
            psWidget, &ProjectStructureWidget::refreshRecentProjects);

    // 已打开文件列表：同步 EditorManager 状态
    connect(editor_manager_, &EditorManager::fileOpened, psWidget,
            &ProjectStructureWidget::onFileOpened);
    connect(editor_manager_, &EditorManager::fileClosed, psWidget,
            &ProjectStructureWidget::onFileClosed);
    // 记录最近文件
    connect(editor_manager_, &EditorManager::fileOpened, this,
            [this](const QString& path) {
              auto& cfg = ConfigManager::instance();
              QStringList files = cfg.get<QStringList>(
                  QString::fromLatin1(CONFIG_RECENT_FILE_LIST));
              files.removeAll(path);
              files.prepend(path);
              while (files.size() > CONFIG_RECENT_FILE_MAX_COUNT)
                files.removeLast();
              cfg.set(QString::fromLatin1(CONFIG_RECENT_FILE_LIST), files);

              QVariantMap timestamps = cfg.get<QVariantMap>(
                  QString::fromLatin1(CONFIG_RECENT_FILE_TIMESTAMPS));
              timestamps.insert(path, QDateTime::currentDateTime());
              cfg.set(QString::fromLatin1(CONFIG_RECENT_FILE_TIMESTAMPS),
                      timestamps);

              updateRecentFilesMenu();
            });
    connect(editor_manager_, &EditorManager::fileClosed, this,
            [this]() { updateRecentFilesMenu(); });
    connect(&projectMgr, &etest::core::project::ProjectManager::projectOpened,
            psWidget, [this, psWidget]() {
              psWidget->setOpenFiles(editor_manager_->openFiles());
            });
    connect(&projectMgr, &etest::core::project::ProjectManager::projectClosed,
            psWidget, [psWidget]() { psWidget->setOpenFiles({}); });
    // 点击已打开文件 → 激活编辑器
    connect(psWidget, &ProjectStructureWidget::openFileActivateRequested, this,
            [this](const QString& path) { editor_manager_->openFile(path); });
    // 右键关闭已打开文件
    connect(psWidget, &ProjectStructureWidget::openFileCloseRequested, this,
            [this](const QString& path) { editor_manager_->closeFile(path); });
    // 点击最近文件 → 项目检测 + 打开
    connect(
        psWidget, &ProjectStructureWidget::recentFileOpenRequested, this,
        [this](const QString& path) {
          QFileInfo fi(path);
          if (!fi.exists()) {
            // 文件已不存在，从最近文件列表中移除
            auto& cfg = ConfigManager::instance();
            QStringList files = cfg.get<QStringList>(
                QString::fromLatin1(CONFIG_RECENT_FILE_LIST));
            if (files.removeAll(path) > 0) {
              cfg.set(QString::fromLatin1(CONFIG_RECENT_FILE_LIST), files);
            }
            return;
          }

          // 向上遍历找 .etproj 项目文件
          QString projFile = findProjectFile(fi.absolutePath());
          if (projFile.isEmpty()) {
            editor_manager_->openFile(path);
            return;
          }

          QFileInfo projFi(projFile);
          if (projFi.absolutePath() ==
              etest::core::project::ProjectManager::instance()
                  .currentProjectRoot()) {
            editor_manager_->openFile(path);
            return;
          }

          // 检查自动打开项目的配置
          auto& cfg = ConfigManager::instance();
          bool autoOpenProject = cfg.get<bool>(
              QString::fromLatin1(CONFIG_RECENT_FILE_AUTO_OPEN_PROJECT),
              CONFIG_RECENT_FILE_AUTO_OPEN_PROJECT_DEFAULT);
          auto& projMgr = etest::core::project::ProjectManager::instance();

          // 如果当前有项目打开，先关闭
          if (projMgr.isProjectOpen()) {
            if (!tryCloseCurrentProject())
              return;
          }

          if (autoOpenProject) {
            projMgr.openProject(projFile);
            editor_manager_->openFile(path);
            return;
          }

          // 不同项目 → 弹对话框询问（含复选框）
          QMessageBox msgBox;
          msgBox.setWindowTitle(QStringLiteral("打开文件"));
          msgBox.setText(
              QStringLiteral("此文件属于项目 \"%1\"，是否打开该项目？")
                  .arg(projFi.completeBaseName()));
          msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
          msgBox.setDefaultButton(QMessageBox::Yes);

          auto* cb =
              new QCheckBox(QStringLiteral("以后始终打开所属项目，不再询问"));
          msgBox.setCheckBox(cb);

          if (msgBox.exec() == QMessageBox::Yes) {
            if (cb->isChecked()) {
              cfg.set(QString::fromLatin1(CONFIG_RECENT_FILE_AUTO_OPEN_PROJECT),
                      true);
            }
            projMgr.openProject(projFile);
          }
          editor_manager_->openFile(path);
        });

    // 拓扑管理器：双击文件打开编辑器
    if (auto* topoMgr = sidebar_->topologyManager()) {
      connect(topoMgr, &TopologyManagerWidget::openFileRequested, topoMgr,
              [this](const QString& path) { editor_manager_->openFile(path); });
    }

    // 文件系统监控：目录内容变化时刷新对应管理器
    connect(psWidget, &ProjectStructureWidget::directoryContentChanged, this,
            [this](const QString& dirPath) {
              if (dirPath.endsWith(QStringLiteral("/protocol")) ||
                  dirPath.endsWith(QStringLiteral("\\protocol"))) {
                sidebar_->protocolManager()->refreshList();
              } else if (dirPath.endsWith(QStringLiteral("/cases")) ||
                         dirPath.endsWith(QStringLiteral("\\cases"))) {
                sidebar_->testProgramManager()->refreshList();
              } else if (dirPath.endsWith(QStringLiteral("/topology")) ||
                         dirPath.endsWith(QStringLiteral("\\topology"))) {
                if (auto* tm = sidebar_->topologyManager())
                  tm->refreshList();
              }
            });
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

  // 新打开测试程序编辑器时连接保存信号
  connect(
      editor_manager_, &EditorManager::fileOpened, this,
      [this](const QString& path) {
        auto* ie = editor_manager_->editorById(path);
        if (!ie) {
          return;
        }
        if (auto* te = qobject_cast<TestProgramEditorWidget*>(ie->widget())) {
          connect(te, &TestProgramEditorWidget::programSaved, this,
                  &MainWindow::onProgramSaved, Qt::UniqueConnection);
        }
      });
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
    LOG_INFO("MAIN_UI", "快捷键 Ctrl+B 切换侧边栏");
    if (sidebar_->isContentVisible()) {
      auto sizes = h_splitter_->sizes();
      if (!sizes.isEmpty()) {
        sidebar_expanded_width_ = sizes[0];
        sizes[0] = 0;
        h_splitter_->setSizes(sizes);
      }
      sidebar_->hideContent();
      activity_bar_->clearActivePage();
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
    LOG_INFO("MAIN_UI", "快捷键 Ctrl+Shift+F 全局搜索");
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

  // 项目树硬件节点 → 导航到平台设备树
  {
    auto* hwPsWidget = qobject_cast<ProjectStructureWidget*>(
        sidebar_->pageById(PageId::kProjectOverview));
    if (hwPsWidget) {
      connect(
          hwPsWidget, &ProjectStructureWidget::hardwareDeviceNavigateRequested,
          this, [this](const QString& deviceType, const QString& pluginId) {
            // 侧边栏切换到平台设备树页面
            sidebar_->switchPage(PageId::kHardware);
            if (!sidebar_->isContentVisible()) {
              sidebar_->showContent();
              auto sizes = h_splitter_->sizes();
              if (!sizes.isEmpty()) {
                sizes[0] = sidebar_expanded_width_;
                h_splitter_->setSizes(sizes);
              }
            }
            activity_bar_->setActivePageId(PageId::kHardware);
            // 高亮对应的设备类型
            sidebar_->hardwareTree()->highlightDeviceType(deviceType, pluginId);
          });
    }
  }

  // 协议管理器：双击文件打开编辑器
  auto* protocolMgr = sidebar_->protocolManager();
  connect(
      protocolMgr, &ProtocolManagerWidget::openFileRequested, protocolMgr,
      [this, protocolMgr](const QString& path) {
        // ICDConfig 是配置容器，不打开编辑器，切到 sidebar 协议页
        if (path.contains(QStringLiteral("ICDConfig"), Qt::CaseInsensitive)) {
          if (!sidebar_->isContentVisible()) {
            sidebar_->showContent();
            auto sizes = h_splitter_->sizes();
            if (!sizes.isEmpty()) {
              sizes[0] = sidebar_expanded_width_;
              h_splitter_->setSizes(sizes);
            }
          }
          sidebar_->switchPage(PageId::kProtocol);
          activity_bar_->setActivePageId(PageId::kProtocol);
          return;
        }
        editor_manager_->openFile(path);
      });

  // 协议管理器：双击帧条目 → 打开帧文件并定位到该帧
  connect(
      protocolMgr, &ProtocolManagerWidget::openFrameRequested, this,
      [this](const QString& framePath, int frameId) {
        editor_manager_->openFile(framePath, QStringLiteral("protocol"));
        if (auto* ie = editor_manager_->editorById(framePath)) {
          if (auto* pe = qobject_cast<etest::protocol::ProtocolEditorWidget*>(
                  ie->widget())) {
            pe->navigateToFrame(frameId);
          }
        }
      });

  // 协议管理器：项目关闭时刷新（打开时在 onProjectOpened 中同步刷新）
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

  // 拓扑管理器：项目打开/关闭时刷新
  if (auto* topoMgr = sidebar_->topologyManager()) {
    connect(&projectMgr, &etest::core::project::ProjectManager::projectOpened,
            topoMgr, &TopologyManagerWidget::refreshList);
    connect(&projectMgr, &etest::core::project::ProjectManager::projectClosed,
            topoMgr, &TopologyManagerWidget::refreshList);
  }

  // 日志输出到界面
  auto* qtSink = etest::core::logger::Logger::qtConsoleSink();
  if (qtSink) {
    connect(qtSink, &QtConsoleSink::logMessage, log_panel_,
            &LogOutputPanel::appendLog);
  }

  // Tab 关闭 → 同步 action 状态 + 更新容器显隐
  connect(bottom_container_, &BottomContainerWidget::panelVisibilityChanged,
          this, [this, updateContainerVisibility]() {
            int outIdx = bottom_container_->indexOf(log_panel_);
            int execIdx = bottom_container_->indexOf(execution_output_panel_);
            int probIdx = bottom_container_->indexOf(problems_panel_);
            int termIdx = bottom_container_->indexOf(terminal_panel_);
            if (outIdx >= 0)
              view_output_action_->setChecked(
                  bottom_container_->isPanelVisible(outIdx));
            if (execIdx >= 0)
              view_execution_output_action_->setChecked(
                  bottom_container_->isPanelVisible(execIdx));
            if (probIdx >= 0)
              view_problems_action_->setChecked(
                  bottom_container_->isPanelVisible(probIdx));
            if (termIdx >= 0)
              view_terminal_action_->setChecked(
                  bottom_container_->isPanelVisible(termIdx));
            updateContainerVisibility();
          });

  // 登录认证成功/登出
  connect(
      &AuthService::instance(), &AuthService::loginSucceeded, this,
      [this](const User& user) {
        QString roleStr = (user.role == UserRole::Admin)
                              ? QStringLiteral("Admin")
                              : QStringLiteral("User");
        activity_bar_->setLoginState(true, user.userName, roleStr);

        login_user_info_action_->setText(
            QStringLiteral("%1 (%2)").arg(user.userName).arg(roleStr));
        login_manage_users_action_->setVisible(user.role == UserRole::Admin);
      });

  connect(&AuthService::instance(), &AuthService::loggedOut, this, [this]() {
    activity_bar_->setLoginState(false, QString(), QString());
  });
}

void MainWindow::lazyInit() {
  QElapsedTimer total_timer;
  total_timer.start();

  // 1. 创建 LoadingOverlay 盖住内容区，启动脉冲
  // 注意：parent = this (MainWindow)，手动定位到 v_splitter_ 区域
  // 若 parent 为 v_splitter_，QSplitter 会将其作为 splitter child
  // 布局，覆盖层不会浮在内容区之上
  QElapsedTimer step_timer;
  step_timer.start();
  loading_overlay_ = new LoadingOverlay(this);
  loading_overlay_->setGeometry(
      QRect(v_splitter_->mapTo(this, QPoint(0, 0)), v_splitter_->size()));
  v_splitter_->installEventFilter(loading_overlay_);
  loading_overlay_->startWithTimeout(10000);
  QCoreApplication::processEvents();  // 立即渲染覆盖层
  LOG_INFO("LAZY", "  [1/12] LoadingOverlay: {} ms", step_timer.elapsed());

  // 2. 注册活动栏按钮 + 侧边栏页面 + 立即恢复页面选中状态
  // （三者合一，避免 addPage 自动选中第一页后又切换的闪烁）
  step_timer.restart();
  {
    activity_bar_->addPage(PageId::kProjectOverview, QStringLiteral("项目概览"),
                           QStringLiteral("project"));
    activity_bar_->addPage(PageId::kSearch, QStringLiteral("搜索"),
                           QStringLiteral("search"));
    activity_bar_->addPage(PageId::kTopology, QStringLiteral("拓扑"),
                           QStringLiteral("topo_tap"));
    activity_bar_->addPage(PageId::kHardware, QStringLiteral("硬件"),
                           QStringLiteral("hardware"));
    activity_bar_->addPage(PageId::kProtocol, QStringLiteral("协议"),
                           QStringLiteral("protocol"));
    activity_bar_->addPage(PageId::kTestProgram, QStringLiteral("测试程序"),
                           QStringLiteral("testprogram"));
    activity_bar_->addPage(PageId::kRun, QStringLiteral("运行"),
                           QStringLiteral("debug"));
    activity_bar_->addPage(PageId::kReport, QStringLiteral("报告"),
                           QStringLiteral("report"));
    activity_bar_->addPage(PageId::kGit, QStringLiteral("Git"),
                           QStringLiteral("git"));

    sidebar_->addPage(PageId::kProjectOverview,
                      new ProjectStructureWidget(sidebar_),
                      QStringLiteral("项目概览"));
    sidebar_->addPage(PageId::kSearch, new SearchWidget(sidebar_),
                      QStringLiteral("搜索"));
    sidebar_->addPage(PageId::kTopology, new TopologyManagerWidget(sidebar_),
                      QStringLiteral("拓扑"));
    sidebar_->addPage(PageId::kHardware, new HardwareTreeWidget(sidebar_),
                      QStringLiteral("硬件"));
    sidebar_->addPage(PageId::kProtocol, new ProtocolManagerWidget(sidebar_),
                      QStringLiteral("协议"));
    test_program_mgr_ = new TestProgramManagerWidget(sidebar_);
    sidebar_->addPage(PageId::kTestProgram, test_program_mgr_,
                      QStringLiteral("测试程序"));
    auto* runPanel = new ExecutionDebugWidget(sidebar_);
    sidebar_->addPage(PageId::kRun, runPanel, QStringLiteral("执行调试"));
    execution_debug_widget_ = runPanel;
    sidebar_->addPage(PageId::kReport, new QWidget(sidebar_),
                      QStringLiteral("报告"));
    sidebar_->addPage(PageId::kGit, new GitWidget(sidebar_),
                      QStringLiteral("Git"));

    // 立即覆盖 addPage 的自动选中，恢复保存的页面
    auto& cfg = ConfigManager::instance();
    bool sidebarVisible = cfg.get<bool>(CONFIG_SIDEBAR_VISIBLE, true);
    QString activePage =
        cfg.get<QString>(CONFIG_SIDEBAR_ACTIVE_PAGE, PageId::kProjectOverview);
    if (sidebarVisible && sidebar_->pageById(activePage)) {
      sidebar_->switchPage(activePage);
      activity_bar_->setActivePageId(activePage);
    } else if (!sidebarVisible) {
      activity_bar_->clearActivePage();
    }
  }
  LOG_INFO("LAZY", "  [2/12] 活动栏+侧边栏+恢复: {} ms", step_timer.elapsed());
  QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

  // 3. 创建底部面板（显隐/尺寸在 lazyInit 末尾由 restoreLazyState 恢复）
  step_timer.restart();
  log_panel_ = new LogOutputPanel(this);
  execution_output_panel_ = new ExecutionOutputPanel(this);
  problems_panel_ = new ProblemsPanel(this);
  terminal_panel_ = new TerminalPanel(this);
  bottom_container_->addPanel(QStringLiteral("日志"), log_panel_,
                              QStringLiteral("tab_output"));
  bottom_container_->addPanel(QStringLiteral("输出"), execution_output_panel_,
                              QStringLiteral("tab_output"));
  bottom_container_->addPanel(QStringLiteral("问题"), problems_panel_,
                              QStringLiteral("tab_problems"));
  bottom_container_->addPanel(QStringLiteral("终端"), terminal_panel_,
                              QStringLiteral("tab_terminal"));
  LOG_INFO("LAZY", "  [3/12] 底部面板: {} ms", step_timer.elapsed());
  QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

  // 4. 创建 EditorManager
  step_timer.restart();
  editor_manager_ = new EditorManager(dock_manager_, this);
  LOG_INFO("LAZY", "  [4/12] EditorManager: {} ms", step_timer.elapsed());
  QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

  // 5. 创建 WelcomeWidget 替换中央占位
  step_timer.restart();
  welcome_widget_ = new WelcomeWidget(this);
  central_dock_->setWidget(welcome_widget_);
  welcome_widget_->refreshRecentProjects();
  LOG_INFO("LAZY", "  [5/12] WelcomeWidget: {} ms", step_timer.elapsed());
  QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

  // 6. 连接跨组件信号（此时所有子控件已就绪）
  step_timer.restart();
  initSignalsLate();
  LOG_INFO("LAZY", "  [6/12] initSignalsLate: {} ms", step_timer.elapsed());

  // 7. 初始化认证服务
  step_timer.restart();
  AuthService::instance();
  updateWindowTitle();
  LOG_INFO("LAZY", "  [7/12] AuthService: {} ms", step_timer.elapsed());

  // 8. 加载插件并刷新硬件树
  step_timer.restart();
  {
    auto& pluginMgr = etest::core::plugin::PluginManager::instance();
    pluginMgr.addSearchPath(QCoreApplication::applicationDirPath() +
                            "/plugins");
    pluginMgr.loadAll();
    sidebar_->hardwareTree()->refreshTree();
  }
  LOG_INFO("LAZY", "  [8/12] 插件+硬件: {} ms", step_timer.elapsed());
  QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

  // 9. 发布提示消息（当前演示用，不必打开）
  if (0) {
    step_timer.restart();
    hint_bar_->postHint(QStringLiteral("已打开项目「测试项目」"));
    hint_bar_->postHint(QStringLiteral("编译完成，发现 2 个警告"));
    hint_bar_->postHint(QStringLiteral("有新版本可用，请更新"),
                        QStringLiteral("更新"), [] { /* 占位 */ });
    hint_bar_->postHint(QStringLiteral("文件「test_spec.xml」已自动保存"),
                        QStringLiteral("查看"), [] { /* 占位 */ });
    hint_bar_->postHint(QStringLiteral("远程连接已断开，尝试重连中..."),
                        QStringLiteral("重试"), [] { /* 占位 */ });
    LOG_INFO("LAZY", "  [9/12] 提示消息: {} ms", step_timer.elapsed());
  }

  // 10. 恢复底部面板状态（逐面板可见性 + 容器显隐）
  step_timer.restart();
  {
    auto& cfg = ConfigManager::instance();
    bottom_container_height_ = cfg.get<int>(CONFIG_BOTTOM_PANEL_HEIGHT, 200);
    bool outVis = cfg.get<bool>(CONFIG_BOTTOM_PANEL_LOG_VISIBLE, true);
    bool probVis = cfg.get<bool>(CONFIG_BOTTOM_PANEL_PROBLEMS_VISIBLE, true);
    bool termVis = cfg.get<bool>(CONFIG_BOTTOM_PANEL_TERMINAL_VISIBLE, true);

    int outIdx = bottom_container_->indexOf(log_panel_);
    int execIdx = bottom_container_->indexOf(execution_output_panel_);
    int probIdx = bottom_container_->indexOf(problems_panel_);
    int termIdx = bottom_container_->indexOf(terminal_panel_);
    if (outIdx >= 0)
      bottom_container_->setPanelVisible(outIdx, outVis);
    if (execIdx >= 0)
      bottom_container_->setPanelVisible(execIdx, true);
    if (probIdx >= 0)
      bottom_container_->setPanelVisible(probIdx, probVis);
    if (termIdx >= 0)
      bottom_container_->setPanelVisible(termIdx, termVis);

    view_output_action_->setChecked(outVis);
    view_execution_output_action_->setChecked(true);
    view_problems_action_->setChecked(probVis);
    view_terminal_action_->setChecked(termVis);

    bool execVis = execIdx >= 0 && bottom_container_->isPanelVisible(execIdx);
    bool anyVisible = outVis || probVis || termVis || execVis;
    if (anyVisible) {
      bottom_container_->show();
      auto vSizes = v_splitter_->sizes();
      if (vSizes.size() >= 2 && vSizes[1] <= 0) {
        vSizes[1] = bottom_container_height_;
        v_splitter_->setSizes(vSizes);
      }
    } else {
      bottom_container_->hide();
    }
  }
  LOG_INFO("LAZY", "  [10/12] 恢复底部面板: {} ms", step_timer.elapsed());
  QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

  // 11. 延迟移除覆盖层 — 给脉冲动画留出显示时间
  step_timer.restart();
  connect(loading_overlay_, &LoadingOverlay::finished, this, [this]() {
    loading_overlay_ = nullptr;
    LOG_INFO("MAIN", "懒加载覆盖层已关闭");
  });
  // 延迟 1.5 秒让用户看到脉冲动画后再淡出
  QTimer::singleShot(1500, loading_overlay_, &LoadingOverlay::finish);
  LOG_INFO("LAZY", "  [11/12] 调度覆盖层移除: {} ms", step_timer.elapsed());

  LOG_INFO("LAZY", "[总计] 懒加载核心步骤: {} ms", total_timer.elapsed());

  // 12. Tux 屏保（独立创建，不影响主流程）
  step_timer.restart();
  tux_overlay_ = new TuxSaverOverlay(this);
  connect(tux_overlay_, &TuxSaverOverlay::closed, this,
          [this]() { tux_idle_timer_.restart(); });
  tux_idle_timer_.start();
  tux_idle_check_timer_ = new QTimer(this);
  connect(tux_idle_check_timer_, &QTimer::timeout, this, [this]() {
    if (!tux_overlay_->isVisible() &&
        ConfigManager::instance().get<bool>(CONFIG_TUXSAVER_ENABLED,
                                            CONFIG_TUXSAVER_DEFAULT_ENABLED)) {
      int timeoutMs =
          ConfigManager::instance().get<int>(CONFIG_TUXSAVER_IDLE_TIMEOUT,
                                             CONFIG_TUXSAVER_DEFAULT_TIMEOUT) *
          1000;
      if (tux_idle_timer_.elapsed() > timeoutMs)
        tux_overlay_->activate();
    }
  });
  tux_idle_check_timer_->start(1000);
  qApp->installEventFilter(this);

  connect(&ConfigManager::instance(), &ConfigManager::configChanged, this,
          [this](const QString& key) {
            if (key == QString::fromLatin1(CONFIG_TUXSAVER_ENABLED) &&
                !ConfigManager::instance().get<bool>(
                    CONFIG_TUXSAVER_ENABLED, CONFIG_TUXSAVER_DEFAULT_ENABLED) &&
                tux_overlay_->isVisible()) {
              tux_overlay_->deactivate();
            }
          });
  LOG_INFO("LAZY", "  [12/12] Tux屏保: {} ms", step_timer.elapsed());

  LOG_INFO("LAZY", "[最终] 懒加载全部完成, 总计: {} ms", total_timer.elapsed());
}

void MainWindow::onThemeChanged(bool isDark) {
  // 同步设置对话框样式（QSS 已由 ThemeManager 全局加载到 qApp）
  if (settings_dialog_) {
    settings_dialog_->setStyleSheet(qApp->styleSheet());
  }

  // 先切 Ribbon 主题（可能触发 style recalculation）
  setRibbonTheme(isDark ? SARibbonTheme::RibbonThemeDark2
                        : SARibbonTheme::RibbonThemeOffice2021Blue);

  // 后设 QADS 暗色样式（覆盖 QADS 内置 widget 级 default.css）
  if (dock_manager_) {
    QString adsQss;
    QFile defaultCss(QStringLiteral(":ads/stylesheets/default.css"));
    if (defaultCss.open(QIODevice::ReadOnly | QIODevice::Text)) {
      adsQss = QString::fromUtf8(defaultCss.readAll());
      defaultCss.close();
    }
    if (isDark) {
      QFile darkCss(QStringLiteral(":/resources/styles/ads_dark.qss"));
      if (darkCss.open(QIODevice::ReadOnly | QIODevice::Text)) {
        adsQss += QStringLiteral("\n") + QString::fromUtf8(darkCss.readAll());
        darkCss.close();
      }
    }
    dock_manager_->setStyleSheet(adsQss);
  }
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

  label_engine_state_ = new QLabel(QStringLiteral("空闲"), this);
  statusBar()->addPermanentWidget(label_engine_state_);

  label_exec_stats_ = new QLabel(QStringLiteral("✅ 0  ❌ 0  ⏱ 0s"), this);
  statusBar()->addPermanentWidget(label_exec_stats_);

  statusBar()->clearMessage();
}

void MainWindow::onNewProject() {
  LOG_INFO("MAIN_UI", "点击「新建项目」");
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
  LOG_INFO("MAIN_UI", "点击「打开项目」");
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
    } else {
      sidebar_->switchPage(PageId::kProjectOverview);
      if (!sidebar_->isContentVisible()) {
        sidebar_->showContent();
        auto sizes = h_splitter_->sizes();
        if (!sizes.isEmpty()) {
          sizes[0] = sidebar_expanded_width_;
          h_splitter_->setSizes(sizes);
        }
      }
      activity_bar_->setActivePageId(PageId::kProjectOverview);
    }
  }
}

void MainWindow::onOpenFile() {
  LOG_INFO("MAIN_UI", "点击「打开文件」");
  QStringList exts = EditorFactoryRegistry::registeredExtensions();
  exts.sort();
  QStringList patterns;
  for (const auto& e : exts) {
    patterns << (QStringLiteral("*.") + e);
  }
  QString filter =
      QStringLiteral("所有支持的文件 (%1)").arg(patterns.join(QChar::Space));

  auto& cfg = ConfigManager::instance();
  QString lastPath = cfg.get<QString>(CONFIG_RECENT_LAST_OPEN_PATH);
  if (lastPath.isEmpty()) {
    lastPath = QDir::homePath();
  }

  QString filePath = QFileDialog::getOpenFileName(
      this, QStringLiteral("打开文件"), lastPath, filter);
  if (filePath.isEmpty())
    return;

  cfg.set(CONFIG_RECENT_LAST_OPEN_PATH, QFileInfo(filePath).absolutePath());
  editor_manager_->openFile(filePath);
}

QString MainWindow::findProjectFile(const QString& dirPath) {
  QDir dir(dirPath);
  for (int i = 0; i < 8 && !dir.isRoot(); ++i) {
    QStringList entries = dir.entryList(QStringList{QStringLiteral("*.etproj")},
                                        QDir::Files | QDir::NoDotAndDotDot);
    if (!entries.isEmpty()) {
      return dir.absoluteFilePath(entries.first());
    }
    if (!dir.cdUp())
      break;
  }
  return {};
}

void MainWindow::openRecentProject(const QString& path) {
  LOG_INFO("MAIN_UI", "打开最近项目 [path={}]", path.toStdString());
  if (!tryCloseCurrentProject()) {
    return;
  }

  auto& pm = etest::core::project::ProjectManager::instance();
  if (pm.openProject(path)) {
    sidebar_->switchPage(PageId::kProjectOverview);
    if (!sidebar_->isContentVisible()) {
      sidebar_->showContent();
      auto sizes = h_splitter_->sizes();
      if (!sizes.isEmpty()) {
        sizes[0] = sidebar_expanded_width_;
        h_splitter_->setSizes(sizes);
      }
    }
    activity_bar_->setActivePageId(PageId::kProjectOverview);
    return;
  }

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
  LOG_INFO("MAIN_UI", "点击「关闭项目」");
  tryCloseCurrentProject();
}

void MainWindow::onProjectOpened(const QString& projectPath) {
  LOG_INFO("MAIN_UI", "项目已打开 [path={}]", projectPath.toStdString());
  close_project_action_->setEnabled(true);
  open_file_action_->setEnabled(false);

  // 记录项目打开时间戳
  QVariantMap timestamps = ConfigManager::instance().get<QVariantMap>(
      CONFIG_RECENT_PROJECT_TIMESTAMPS);
  timestamps[projectPath] = QDateTime::currentDateTime();
  ConfigManager::instance().set(CONFIG_RECENT_PROJECT_TIMESTAMPS, timestamps);

  auto& projectMgr = etest::core::project::ProjectManager::instance();
  auto* project = projectMgr.currentProject();
  if (project) {
    status_project_label_->setText(project->name());
  }

  updateWindowTitle();
  status_message_label_->setText(
      QStringLiteral("项目已打开：%1").arg(projectPath));

  // M6: 初始化 SignalRegistry + ICD Repository（同步接合）
  if (!signal_registry_) {
    signal_registry_ = new etest::core::SignalRegistry(this);
  } else {
    signal_registry_->clear();
  }
  editor_manager_->setSignalRegistry(signal_registry_);

  // 同步触发协议管理器刷新（确保 ICD 数据已加载），
  // ProtocolManagerWidget::refreshList 在同一信号链中稍后执行，
  // 这里手动触发一次以保证 onProjectOpened 返回时 ICD 数据就绪
  if (auto* pm = sidebar_->protocolManager()) {
    pm->refreshList();
    icd_repository_ = pm->repository();
  }
  editor_manager_->setIcdRepository(icd_repository_.get());

  // 确保引擎就绪（此时 signal_registry_ 和 icd_repository_ 已可用）
  createEngine();

  if (signal_registry_ && icd_repository_) {
    LOG_DEBUG("UUID", "ICD Repository loaded, frames={}",
              icd_repository_->frames().size());

    // 预注册项目中的已有拓扑设备：扫描 topology/ 目录下所有 .etopo 文件
    const QString topoDir = projectPath + QStringLiteral("/topology");
    QDir topoDirObj(topoDir);
    if (topoDirObj.exists()) {
      for (const QFileInfo& fi : topoDirObj.entryInfoList(
               {QStringLiteral("*.etopo")}, QDir::Files, QDir::Name)) {
        QFile f(fi.absoluteFilePath());
        if (!f.open(QIODevice::ReadOnly)) {
          continue;
        }
        QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        f.close();
        if (!doc.isObject()) {
          continue;
        }
        QJsonArray devices = doc.object()[QStringLiteral("devices")].toArray();
        for (const QJsonValue& dv : devices) {
          QJsonObject dobj = dv.toObject();
          QString id = dobj[QStringLiteral("id")].toString();
          QString name = dobj[QStringLiteral("name")].toString();
          if (id.isEmpty()) {
            id = QUuid::createUuid().toString(QUuid::WithoutBraces);
          }
          signal_registry_->registerDevice(
              id, name,
              dobj[QStringLiteral("type")].toString());
          QJsonArray ports = dobj[QStringLiteral("ports")].toArray();
          for (const QJsonValue& pv : ports) {
            QJsonObject pobj = pv.toObject();
            QStringList frames;
            for (const QJsonValue& fv :
                 pobj[QStringLiteral("boundFrames")].toArray()) {
              frames.append(fv.toString());
            }
            signal_registry_->bindPortToFrames(
                id, pobj[QStringLiteral("name")].toString(), frames);
          }
        }
      }
    }
    // 重建信号索引（含预注册的设备和绑定）
    etest::app::synchronizeRegistry(*signal_registry_, icd_repository_.get());
    LOG_DEBUG("UUID", "after pre-register + synchronizeRegistry: devices={}",
              signal_registry_->registeredDeviceIds().size());

    // 为已打开的测试程序编辑器注入 IcdSignalSelection + 连接保存信号
    for (const QString& path : editor_manager_->openFiles()) {
      auto* ie = editor_manager_->editorById(path);
      if (!ie) {
        continue;
      }
      if (auto* te = qobject_cast<TestProgramEditorWidget*>(ie->widget())) {
        te->setSignalSelection(
            new IcdSignalSelection(signal_registry_, icd_repository_.get()));
        te->setRegistry(signal_registry_);
        connect(te, &TestProgramEditorWidget::programSaved, this,
                &MainWindow::onProgramSaved);
      }
    }

    // 将可用帧名传播给已打开的拓扑编辑器
    QStringList allFrames;
    for (const auto& frame : icd_repository_->frames()) {
      if (!frame) {
        continue;
      }
      auto name = frame->name();
      allFrames.append(
          QString::fromUtf8(name.data(), static_cast<int>(name.size())));
    }
    for (const QString& path : editor_manager_->openFiles()) {
      auto* ie = editor_manager_->editorById(path);
      if (!ie) {
        continue;
      }
      if (auto* topo = qobject_cast<etest::topology::TopologyEditorWidget*>(
              ie->widget())) {
        topo->setAvailableIcdFrames(allFrames);
      }
    }
  }

  // 切换到侧边栏项目管理页面
  sidebar_->switchPage(PageId::kProjectOverview);
  if (!sidebar_->isContentVisible()) {
    sidebar_->showContent();
    auto sizes = h_splitter_->sizes();
    if (!sizes.isEmpty()) {
      sizes[0] = sidebar_expanded_width_;
      h_splitter_->setSizes(sizes);
    }
  }
  activity_bar_->setActivePageId(PageId::kProjectOverview);
}

void MainWindow::onProjectClosed() {
  LOG_INFO("MAIN_UI", "项目已关闭");
  close_project_action_->setEnabled(false);
  open_file_action_->setEnabled(true);
  status_project_label_->setText(QStringLiteral("无打开项目"));
  updateWindowTitle();
  status_message_label_->setText(QStringLiteral("项目已关闭"));

  // M6: 清理 ICD 上下文
  destroyEngine();

  if (signal_registry_) {
    signal_registry_->clear();
  }
  editor_manager_->setSignalRegistry(nullptr);
  editor_manager_->setIcdRepository(nullptr);
  icd_repository_ = nullptr;
}

void MainWindow::updateWindowTitle() {
  auto& projectMgr = etest::core::project::ProjectManager::instance();
  if (projectMgr.isProjectOpen()) {
    auto* project = projectMgr.currentProject();
    QString title = project->name();
    if (projectMgr.hasUnsavedChanges()) {
      title.prepend("* ");
    }
    title += " - ETestStudio";
    setWindowTitle(title);
  } else {
    setWindowTitle(QStringLiteral("ETestStudio"));
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

void MainWindow::updateRecentFilesMenu() {
  recent_files_menu_->clear();

  auto& cfg = ConfigManager::instance();
  QStringList files =
      cfg.get<QStringList>(QString::fromLatin1(CONFIG_RECENT_FILE_LIST));

  if (files.isEmpty()) {
    auto* emptyAction = recent_files_menu_->addAction(QStringLiteral("（无）"));
    emptyAction->setEnabled(false);
  } else {
    for (const QString& path : files) {
      recent_files_menu_->addAction(
          path, this, [this, path]() { editor_manager_->openFile(path); });
    }
    recent_files_menu_->addSeparator();
    recent_files_menu_->addAction(
        QStringLiteral("清除最近文件"), this, [this]() {
          auto& cfg = ConfigManager::instance();
          cfg.set(QString::fromLatin1(CONFIG_RECENT_FILE_LIST), QStringList());
          cfg.set(QString::fromLatin1(CONFIG_RECENT_FILE_TIMESTAMPS),
                  QVariantMap());
          updateRecentFilesMenu();
        });
  }
}

void MainWindow::onSaveFile() {
  LOG_INFO("MAIN_UI", "点击「保存」");
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
  LOG_INFO("MAIN_UI", "点击「另存为」");
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
  LOG_INFO("MAIN_UI", "点击「保存所有」");
  editor_manager_->saveAllFiles();
}

void MainWindow::onCloseCurrentFile() {
  LOG_INFO("MAIN_UI", "点击「关闭文件」");
  auto* editor = editor_manager_->currentEditor();
  if (!editor)
    return;
  editor_manager_->closeFile(editor->editorId());
}

void MainWindow::onCloseAllFiles() {
  LOG_INFO("MAIN_UI", "点击「关闭所有文件」");
  editor_manager_->closeAllFiles();
}

void MainWindow::onUndo() {
  LOG_INFO("MAIN_UI", "点击「撤销」");
  if (auto* editor = editor_manager_->currentEditor()) {
    editor->undo();
  }
}

void MainWindow::onRedo() {
  LOG_INFO("MAIN_UI", "点击「重做」");
  if (auto* editor = editor_manager_->currentEditor()) {
    editor->redo();
  }
}

void MainWindow::onCut() {
  LOG_INFO("MAIN_UI", "点击「剪切」");
  if (auto* editor = editor_manager_->currentEditor()) {
    if (auto* textEditor = dynamic_cast<TextEditorWidget*>(editor)) {
      textEditor->editor()->cut();
    }
  }
}

void MainWindow::onCopy() {
  LOG_INFO("MAIN_UI", "点击「复制」");
  if (auto* editor = editor_manager_->currentEditor()) {
    if (auto* textEditor = dynamic_cast<TextEditorWidget*>(editor)) {
      textEditor->editor()->copy();
    }
  }
}

void MainWindow::onPaste() {
  LOG_INFO("MAIN_UI", "点击「粘贴」");
  if (auto* editor = editor_manager_->currentEditor()) {
    if (auto* textEditor = dynamic_cast<TextEditorWidget*>(editor)) {
      textEditor->editor()->paste();
    }
  }
}

void MainWindow::onFind() {
  LOG_INFO("MAIN_UI", "点击「查找」");
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
  LOG_INFO("MAIN_UI", "点击「转到行」");
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

// ── 转换工具：etest::app::TestProgramData → etest::engine::ProgramData ──
namespace {

etest::engine::TestStepData convertStep(const etest::app::TestStepData& src) {
  etest::engine::TestStepData dst;
  dst.command = src.cmd;
  dst.target = src.target;
  dst.value = src.value.toDouble();
  dst.tolerance = src.tolerance.enabled ? src.tolerance.max : 0.0;
  dst.extra = src.description;
  dst.timeoutMs = src.timeoutMs;
  dst.loopCount = src.loopCount;
  // 转换条件表达式到字符串形式
  if (src.condition.target.isEmpty()) {
    dst.condition.clear();
  } else {
    dst.condition = src.condition.target + QStringLiteral(" ") +
                    src.condition.op + QStringLiteral(" ") +
                    src.condition.value.toString();
  }
  // 递归转换子步骤
  for (const auto& ss : src.subSteps) {
    dst.subSteps.append(convertStep(ss));
  }
  for (const auto& es : src.elseSubSteps) {
    dst.elseSteps.append(convertStep(es));
  }
  return dst;
}

etest::engine::TestCaseData convertCase(const etest::app::TestCaseData& src) {
  etest::engine::TestCaseData dst;
  dst.caseName = src.name;
  for (const auto& step : src.steps) {
    dst.steps.append(convertStep(step));
  }
  return dst;
}

etest::engine::ProgramData convertProgram(
    const etest::app::TestProgramData& src) {
  etest::engine::ProgramData dst;
  dst.suiteName = src.name;
  for (const auto& tc : src.cases) {
    dst.cases.append(convertCase(tc));
  }
  return dst;
}

}  // anonymous namespace

// ── 引擎生命周期 ──

void MainWindow::createEngine() {
  if (engine_) {
    return;
  }
  engine_ = new etest::engine::TestExecutionEngine(signal_registry_,
                                                   icd_repository_.get(), this);
  // 连接发动机状态变更信号 → Ribbon 同步 + StatusBar
  connect(engine_, &etest::engine::TestExecutionEngine::engineStateChanged,
          this, [this](etest::engine::EngineState state) {
            syncControlStates();
            QString stateText;
            switch (state) {
              case etest::engine::EngineState::Idle:
                stateText = QStringLiteral("就绪");
                statusBar()->showMessage(stateText);
                break;
              case etest::engine::EngineState::Running:
                stateText = QStringLiteral("运行中");
                statusBar()->showMessage(QStringLiteral("运行中..."));
                break;
              case etest::engine::EngineState::Paused:
                stateText = QStringLiteral("已暂停");
                statusBar()->showMessage(stateText);
                break;
              case etest::engine::EngineState::Finished:
                stateText = QStringLiteral("已完成");
                statusBar()->showMessage(
                    QStringLiteral("执行完成 (✅%1 ❌%2)")
                        .arg(pass_count_).arg(fail_count_));
                break;
              case etest::engine::EngineState::Error:
                stateText = QStringLiteral("错误");
                statusBar()->showMessage(QStringLiteral("执行出错"));
                break;
            }
            if (label_engine_state_) {
              label_engine_state_->setText(stateText);
            }
          });

  // 绑定到监视面板（bindEngine 内部连接所有需要的信号）
  execution_debug_widget_->bindEngine(engine_);

  // suiteFinished → 累计统计 + 更新 StatusBar
  connect(engine_, &etest::engine::TestExecutionEngine::suiteFinished,
          this, [this](const QString& /*name*/, int pass, int fail) {
    pass_count_ += pass;
    fail_count_ += fail;
    if (label_exec_stats_) {
      label_exec_stats_->setText(
          QStringLiteral("✅ %1  ❌ %2  ⏱ %3s")
              .arg(pass_count_).arg(fail_count_)
              .arg(0));
    }
  });

  // 步骤结果 → 执行输出面板
  connect(engine_, &etest::engine::TestExecutionEngine::stepFinished, this,
          [this](int /*caseIndex*/, const QString& /*stepPath*/,
                 const etest::engine::StepResult& result) {
            execution_output_panel_->appendResult(result);
          });

  // 引擎级错误 → 执行输出面板
  connect(engine_, &etest::engine::TestExecutionEngine::engineError, this,
          [this](const QString& msg) {
            execution_output_panel_->appendError(msg);
          });

  // 引擎执行完成后保存 .etlog 报告
  connect(engine_, &etest::engine::TestExecutionEngine::engineFinished, this,
          [this]() {
            if (current_program_name_.isEmpty()) {
              return;
            }
            auto& projMgr = etest::core::project::ProjectManager::instance();
            QString reportDir =
                projMgr.currentProjectRoot() + QStringLiteral("/reports");
            QDir().mkpath(reportDir);
            QString etlogPath = reportDir + QStringLiteral("/") +
                                current_program_name_ +
                                QStringLiteral(".etlog");
            engine_->saveReport(etlogPath);
          });
}

void MainWindow::destroyEngine() {
  if (!engine_) {
    return;
  }
  engine_->stop();
  engine_->deleteLater();
  engine_ = nullptr;
}

void MainWindow::syncControlStates() {
  if (!engine_) {
    act_run_->setEnabled(true);
    act_run_all_->setEnabled(true);
    act_pause_->setEnabled(false);
    act_stop_->setEnabled(false);
    act_verify_->setEnabled(true);
    return;
  }
  auto state = engine_->state();
  switch (state) {
    case etest::engine::EngineState::Idle:
    case etest::engine::EngineState::Finished:
      act_run_->setEnabled(true);
      act_run_all_->setEnabled(true);
      act_pause_->setEnabled(false);
      act_stop_->setEnabled(false);
      act_verify_->setEnabled(true);
      break;
    case etest::engine::EngineState::Running:
      act_run_->setEnabled(false);
      act_run_all_->setEnabled(false);
      act_pause_->setEnabled(true);
      act_pause_->setText(QStringLiteral("暂停"));
      act_pause_->setIcon(
          AppIconProvider::instance().icon(QStringLiteral("pause")));
      act_stop_->setEnabled(true);
      act_verify_->setEnabled(false);
      break;
    case etest::engine::EngineState::Paused:
      act_run_->setEnabled(false);
      act_run_all_->setEnabled(false);
      act_pause_->setEnabled(true);
      act_pause_->setText(QStringLiteral("继续"));
      act_pause_->setIcon(
          AppIconProvider::instance().icon(QStringLiteral("resume")));
      act_stop_->setEnabled(true);
      act_verify_->setEnabled(false);
      break;
    case etest::engine::EngineState::Error:
      act_run_->setEnabled(true);
      act_run_all_->setEnabled(true);
      act_pause_->setEnabled(false);
      act_stop_->setEnabled(false);
      act_verify_->setEnabled(true);
      break;
  }
}

// ── Ribbon 运行按钮 ──

void MainWindow::onRunClicked() {
  LOG_INFO("MAIN_UI", "点击「运行」");

  // 0. 前提检查
  if (execution_debug_widget_ && !execution_debug_widget_->canRun()) {
    QMessageBox::warning(this, QStringLiteral("运行"),
                         QStringLiteral("运行前提不满足，请先执行验证"));
    return;
  }

  // 1. 先切侧边栏到执行调试页面（视觉反馈优先）
  sidebar_->switchPage(PageId::kRun);
  if (!sidebar_->isContentVisible()) {
    sidebar_->showContent();
    auto sizes = h_splitter_->sizes();
    if (!sizes.isEmpty()) {
      sizes[0] = sidebar_expanded_width_;
      h_splitter_->setSizes(sizes);
    }
  }
  activity_bar_->setActivePageId(PageId::kRun);

  // 2. 获取测试程序数据：优先从编辑器取，其次从 TestProgramManagerWidget 取
  etest::app::TestProgramData data;
  auto* editor = editor_manager_->currentEditor();
  auto* progEditor = qobject_cast<TestProgramEditorWidget*>(
      editor ? editor->widget() : nullptr);
  if (progEditor) {
    // 2a. 从编辑器取
    if (editor->isModified()) {
      editor->save();
    }
    data = progEditor->programData();
  } else {
    // 2b. 从 TestProgramManagerWidget 取选中项
    data = test_program_mgr_->loadSelectedProgramData();
    if (data.name.isEmpty()) {
      QMessageBox::information(
          this, QStringLiteral("运行"),
          QStringLiteral("请先打开一个测试程序或从测试程序列表中选中一个"));
      return;
    }
  }

  if (data.cases.isEmpty()) {
    QMessageBox::warning(this, QStringLiteral("运行"),
                         QStringLiteral("测试程序中没有测试用例，无法运行"));
    return;
  }

  // 3. 创建引擎（若需要）
  createEngine();

  // 3b. 同步 registry
  engine_->setRegistry(signal_registry_, icd_repository_.get());

  // 3c. 加载拓扑设备
  {
    auto& projMgr = ProjectManager::instance();
    if (projMgr.isProjectOpen()) {
      QString topoDir = projMgr.currentProjectRoot() + QStringLiteral("/topology");
      QDir topoDirObj(topoDir);
      if (topoDirObj.exists()) {
        const auto topoFiles = topoDirObj.entryInfoList(
            {QStringLiteral("*.etopo")}, QDir::Files, QDir::Name);
        for (const QFileInfo& fi : topoFiles) {
          engine_->loadTopology(fi.absoluteFilePath());
        }
      }
    }
  }

  // 4. 设置程序数据
  current_program_name_ = data.name;
  engine_->setProgram(convertProgram(data));

  // 重置统计计数
  pass_count_ = 0;
  fail_count_ = 0;
  if (label_exec_stats_) {
    label_exec_stats_->setText(QStringLiteral("✅ 0  ❌ 0  ⏱ 0s"));
  }

  // 5. 启动
  engine_->start();
}

void MainWindow::onPauseClicked() {
  if (!engine_) {
    return;
  }
  if (engine_->state() == etest::engine::EngineState::Running) {
    LOG_INFO("MAIN_UI", "点击「暂停」");
    engine_->pause();
  } else if (engine_->state() == etest::engine::EngineState::Paused) {
    LOG_INFO("MAIN_UI", "点击「继续」");
    engine_->resume();
  }
}

void MainWindow::onStopClicked() {
  LOG_INFO("MAIN_UI", "点击「停止」");
  if (engine_) {
    engine_->stop();
  }
}

void MainWindow::onVerifyClicked() {
  LOG_INFO("MAIN_UI", "点击「校验」");
  if (!problems_panel_) {
    return;
  }
  auto* problems = problems_panel_;
  problems->clearProblems();
  int errors = 0;
  int warnings = 0;

  // 1. 项目已打开
  auto& projMgr = ProjectManager::instance();
  bool projectOpen = projMgr.isProjectOpen();
  if (!projectOpen) {
    problems->addProblem(QStringLiteral("运行"), QStringLiteral("错误"),
                         QStringLiteral("未打开项目"));
    errors++;
  }

  // 2. ICD 协议已定义
  bool icdLoaded = icd_repository_ && !icd_repository_->frames().empty();
  if (!icdLoaded) {
    problems->addProblem(QStringLiteral("运行"), QStringLiteral("错误"),
                         QStringLiteral("ICD 协议未加载"));
    errors++;
  }

  // 3. 拓扑已配置
  bool topoExists = false;
  if (projectOpen) {
    QString topoDir = projMgr.currentProjectRoot() + QStringLiteral("/topology");
    QDir topoDirObj(topoDir);
    topoExists = topoDirObj.exists()
                 && !topoDirObj.entryList({QStringLiteral("*.etopo")},
                                           QDir::Files).isEmpty();
  }
  if (!topoExists) {
    problems->addProblem(QStringLiteral("运行"), QStringLiteral("错误"),
                         QStringLiteral("未找到拓扑文件"));
    errors++;
  }

  // 4. 拓扑已绑定信号
  bool signalBound = signal_registry_
                     && !signal_registry_->registeredDeviceIds().isEmpty();
  if (!signalBound) {
    problems->addProblem(QStringLiteral("运行"), QStringLiteral("警告"),
                         QStringLiteral("拓扑未绑定信号"));
    warnings++;
  }

  // 5. 测试程序可用：优先编辑器，退回到选中项
  bool hasProgram = false;
  auto* editor = editor_manager_->currentEditor();
  auto* progEditor = editor
      ? qobject_cast<TestProgramEditorWidget*>(editor->widget()) : nullptr;
  if (progEditor) {
    hasProgram = !progEditor->programData().cases.isEmpty();
  } else if (test_program_mgr_) {
    auto data = test_program_mgr_->loadSelectedProgramData();
    hasProgram = !data.cases.isEmpty();
  }
  if (!hasProgram) {
    problems->addProblem(QStringLiteral("运行"), QStringLiteral("错误"),
                         QStringLiteral("未选择有效的测试程序"));
    errors++;
  }

  // 6. 硬件/Mock 状态
  // TODO: 接入硬件管理器设备状态查询
  if (!topoExists && !signalBound) {
    problems->addProblem(QStringLiteral("运行"), QStringLiteral("警告"),
                         QStringLiteral("硬件/Mock 未配置"));
    warnings++;
  }

  problems->showSummary(errors, warnings);

  // 有问题时自动切换到问题面板
  if (errors > 0 || warnings > 0) {
    int idx = bottom_container_->indexOf(problems_panel_);
    if (idx >= 0) {
      bottom_container_->setCurrentPanel(idx);
      bottom_container_->show();
    }
  }

  // 同步 ExecutionDebugWidget 依赖并刷新概览区
  if (execution_debug_widget_) {
    execution_debug_widget_->setDependencies(signal_registry_,
                                              icd_repository_.get());
  }
}

void MainWindow::onRunAllClicked() {
  LOG_INFO("MAIN_UI", "点击「运行全部」");
  // 运行全部：委托给 onRunClicked
  onRunClicked();
}

void MainWindow::onProgramSaved(const QString& path) {
  // 程序保存后的回调：可在此更新引擎数据或状态
  Q_UNUSED(path);
}

void MainWindow::resizeEvent(QResizeEvent* event) {
  SARibbonMainWindow::resizeEvent(event);
  if (loading_overlay_ && loading_overlay_->isVisible()) {
    loading_overlay_->setGeometry(
        QRect(v_splitter_->mapTo(this, QPoint(0, 0)), v_splitter_->size()));
    loading_overlay_->raise();
  }
}

void MainWindow::showEvent(QShowEvent* event) {
  SARibbonMainWindow::showEvent(event);
  if (first_show_) {
    first_show_ = false;
    onThemeChanged(ThemeManager::instance().isDarkTheme());
  }
}

void MainWindow::closeEvent(QCloseEvent* event) {
  // 尝试关闭所有编辑器文件，如果用户取消则不关闭程序
  if (!editor_manager_->closeAllFiles()) {
    event->ignore();
    return;
  }

  // 关闭屏保
  if (tux_overlay_ && tux_overlay_->isVisible()) {
    tux_overlay_->deactivate();
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

  open_file_action_ = new QAction(QStringLiteral("打开文件"), this);
  open_file_action_->setShortcut(QStringLiteral("Ctrl+Shift+O"));
  open_file_action_->setShortcutContext(Qt::ApplicationShortcut);
  open_file_action_->setIcon(
      AppIconProvider::instance().icon(QStringLiteral("file_open")));
  open_file_action_->setEnabled(true);
  connect(open_file_action_, &QAction::triggered, this,
          &MainWindow::onOpenFile);

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
  new_project_action_->setIcon(
      AppIconProvider::instance().icon(QStringLiteral("file_new")));
  qab->addAction(new_project_action_);
  open_project_action_->setIcon(
      AppIconProvider::instance().icon(QStringLiteral("project_open")));
  qab->addAction(open_project_action_);
  save_action_->setIcon(
      AppIconProvider::instance().icon(QStringLiteral("file_save")));
  qab->addAction(save_action_);
  qab->addSeparator();

  edit_undo_action_->setIcon(
      AppIconProvider::instance().icon(QStringLiteral("undo")));
  qab->addAction(edit_undo_action_);
  edit_redo_action_->setIcon(
      AppIconProvider::instance().icon(QStringLiteral("redo")));
  qab->addAction(edit_redo_action_);

  // ── 登录菜单 ──
  login_menu_ = new QMenu(this);
  login_user_info_action_ =
      login_menu_->addAction(QStringLiteral("admin (Admin)"));
  login_user_info_action_->setEnabled(false);
  login_menu_->addSeparator();
  login_manage_users_action_ =
      login_menu_->addAction(QStringLiteral("用户管理"));
  connect(login_manage_users_action_, &QAction::triggered, this, [this]() {
    LOG_INFO("MAIN_UI", "点击「用户管理」");
    UserManagerDialog dlg(this);
    dlg.exec();
  });
  login_menu_->addSeparator();
  auto* logoutAction = login_menu_->addAction(QStringLiteral("退出登录"));
  connect(logoutAction, &QAction::triggered, this,
          [this]() {
            LOG_INFO("MAIN_UI", "点击「退出登录」");
            AuthService::instance().logout();
          });

  // ---- Application Button ----
  ribbon->applicationButton()->setIcon(
      AppIconProvider::instance().icon(QStringLiteral("file_new")));
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

  recent_files_menu_ = app_menu->addMenu(QStringLiteral("最近文件"));
  updateRecentFilesMenu();

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
    panel_file->addLargeAction(open_file_action_);
    panel_file->addLargeAction(save_action_);

    // 编辑 Panel
    auto* panel_edit = cat->addPanel(QStringLiteral("编辑"));
    panel_edit->addLargeAction(edit_undo_action_);
    panel_edit->addLargeAction(edit_redo_action_);
    panel_edit->addSeparator();

    edit_cut_action_->setIcon(
        AppIconProvider::instance().icon(QStringLiteral("file_cut")));
    edit_copy_action_->setIcon(
        AppIconProvider::instance().icon(QStringLiteral("file_copy")));
    edit_paste_action_->setIcon(
        AppIconProvider::instance().icon(QStringLiteral("file_paste")));
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
    act_welcome->setIcon(
        AppIconProvider::instance().icon(QStringLiteral("welcome")));
    connect(act_welcome, &QAction::triggered, this, [this]() {
      LOG_INFO("MAIN_UI", "点击 Ribbon「欢迎页」");
      auto* centralDock = dock_manager_->findDockWidget("CentralDock");
      if (!centralDock)
        return;
      if (centralDock->isClosed())
        centralDock->toggleView(true);
      if (auto* area = centralDock->dockAreaWidget())
        area->setCurrentIndex(0);
    });
    panel_panels->addLargeAction(act_welcome);

    view_output_action_ = new QAction(QStringLiteral("日志"), this);
    view_output_action_->setIcon(
        AppIconProvider::instance().icon(QStringLiteral("tab_output")));
    view_output_action_->setCheckable(true);
    view_output_action_->setChecked(true);
    panel_panels->addLargeAction(view_output_action_);

    view_execution_output_action_ = new QAction(QStringLiteral("输出"), this);
    view_execution_output_action_->setIcon(
        AppIconProvider::instance().icon(QStringLiteral("tab_output")));
    view_execution_output_action_->setCheckable(true);
    view_execution_output_action_->setChecked(true);
    panel_panels->addLargeAction(view_execution_output_action_);

    view_problems_action_ = new QAction(QStringLiteral("问题"), this);
    view_problems_action_->setIcon(
        AppIconProvider::instance().icon(QStringLiteral("tab_problems")));
    view_problems_action_->setCheckable(true);
    view_problems_action_->setChecked(true);
    panel_panels->addLargeAction(view_problems_action_);

    view_terminal_action_ = new QAction(QStringLiteral("终端"), this);
    view_terminal_action_->setIcon(
        AppIconProvider::instance().icon(QStringLiteral("tab_terminal")));
    view_terminal_action_->setCheckable(true);
    view_terminal_action_->setChecked(true);
    panel_panels->addLargeAction(view_terminal_action_);

    view_aux_sidebar_action_ = new QAction(QStringLiteral("辅助侧边栏"), this);
    view_aux_sidebar_action_->setIcon(
        AppIconProvider::instance().icon(QStringLiteral("sidebar")));
    view_aux_sidebar_action_->setCheckable(true);
    view_aux_sidebar_action_->setChecked(false);
    panel_panels->addLargeAction(view_aux_sidebar_action_);
  }

  // ============================================================
  //  运行
  // ============================================================
  {
    auto* cat = ribbon->addCategoryPage(QStringLiteral("运行"));

    // 执行控制 Panel
    auto* panel_control = cat->addPanel(QStringLiteral("执行控制"));
    act_run_ =
        new QAction(AppIconProvider::instance().icon(QStringLiteral("run")),
                    QStringLiteral("运行"), this);
    act_pause_ =
        new QAction(AppIconProvider::instance().icon(QStringLiteral("pause")),
                    QStringLiteral("暂停"), this);
    act_stop_ =
        new QAction(AppIconProvider::instance().icon(QStringLiteral("stop")),
                    QStringLiteral("停止"), this);
    act_verify_ =
        new QAction(AppIconProvider::instance().icon(QStringLiteral("verify")),
                    QStringLiteral("验证"), this);
    panel_control->addLargeAction(act_verify_);
    panel_control->addLargeAction(act_run_);
    panel_control->addSmallAction(act_pause_);
    panel_control->addSmallAction(act_stop_);

    // 连接 Ribbon 运行按钮
    connect(act_run_, &QAction::triggered, this, &MainWindow::onRunClicked);
    connect(act_pause_, &QAction::triggered, this, &MainWindow::onPauseClicked);
    connect(act_stop_, &QAction::triggered, this, &MainWindow::onStopClicked);
    connect(act_verify_, &QAction::triggered, this,
            &MainWindow::onVerifyClicked);

    // 运行方式 Panel
    auto* panel_mode = cat->addPanel(QStringLiteral("运行方式"));
    act_run_all_ =
        new QAction(AppIconProvider::instance().icon(QStringLiteral("run_all")),
                    QStringLiteral("运行全部"), this);
    panel_mode->addLargeAction(act_run_all_);

    // 连接运行方式按钮
    connect(act_run_all_, &QAction::triggered, this,
            &MainWindow::onRunAllClicked);

    // 统计 Panel
    auto* panel_stats = cat->addPanel(QStringLiteral("统计"));
    label_ribbon_stats_ = new QLabel(QStringLiteral("✅ 0  ❌ 0  ⏱ 0"), this);
    panel_stats->addSmallWidget(label_ribbon_stats_);
  }

  // ============================================================
  //  工具
  // ============================================================
  {
    auto* cat = ribbon->addCategoryPage(QStringLiteral("工具"));

    auto* panel_tools = cat->addPanel(QStringLiteral("工具"));
    auto* act_settings = new QAction(
        AppIconProvider::instance().icon(QStringLiteral("ribbon_settings")),
        QStringLiteral("设置"), this);
    connect(act_settings, &QAction::triggered, this, [this]() {
      LOG_INFO("MAIN_UI", "点击 Ribbon「设置」");
      if (!settings_dialog_) {
        settings_dialog_ = new SettingsDialog(this);
        settings_dialog_->setStyleSheet(qApp->styleSheet());
        connect(settings_dialog_, &QDialog::finished, this,
                [this]() { activity_bar_->setSettingsActive(false); });
      }
      activity_bar_->setSettingsActive(true);
      settings_dialog_->show();
      settings_dialog_->raise();
      settings_dialog_->activateWindow();
    });
    panel_tools->addLargeAction(act_settings);

    panel_tools->addSeparator();

    auto addDemoAction = [&](QAction*& act, const QString& name,
                             const QString& exeName, const QString& iconName) {
      act = new QAction(name, this);
      act->setIcon(AppIconProvider::instance().icon(iconName));
      QObject::connect(act, &QAction::triggered, this, [exeName, name]() {
        LOG_INFO("MAIN_UI", "启动独立工具「{}」 [exe={}]", name.toStdString(),
                 exeName.toStdString());
        QString path =
            QApplication::applicationDirPath() + QStringLiteral("/") + exeName;
        if (!QProcess::startDetached(path)) {
          QMessageBox::warning(
              nullptr, QStringLiteral("启动失败"),
              QStringLiteral("无法启动 %1\n路径: %2").arg(exeName, path));
        }
      });
      panel_tools->addLargeAction(act);
    };
    addDemoAction(demo_topology_action_, QStringLiteral("拓扑编辑器"),
                  QStringLiteral("topology-editor.exe"),
                  QStringLiteral("ribbon_topology"));
    addDemoAction(demo_protocol_action_, QStringLiteral("帧协议编辑器"),
                  QStringLiteral("protocol-editor.exe"),
                  QStringLiteral("ribbon_protocol"));
    addDemoAction(demo_testprogram_action_, QStringLiteral("测试程序编辑器"),
                  QStringLiteral("test-program-editor.exe"),
                  QStringLiteral("ribbon_testprogram"));
    addDemoAction(demo_testexecutor_action_, QStringLiteral("测试执行器"),
                  QStringLiteral("test-executor.exe"),
                  QStringLiteral("ribbon_testexecutor"));
  }

  // ============================================================
  //  帮助
  // ============================================================
  {
    auto* cat = ribbon->addCategoryPage(QStringLiteral("帮助"));

    auto* panel_about = cat->addPanel(QStringLiteral("关于"));
    auto* act_about = new QAction(
        AppIconProvider::instance().icon(QStringLiteral("ribbon_about")),
        QStringLiteral("关于 ETest Demo"), this);
    connect(act_about, &QAction::triggered, this, [this]() {
      LOG_INFO("MAIN_UI", "点击 Ribbon「关于」");
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

  // 设置 Ribbon 运行按钮的初始状态
  syncControlStates();
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

  // sidebar state — 保存侧边栏内部页面 ID（即使用户折叠了侧边栏也有有效值）
  cfg.set(CONFIG_SIDEBAR_ACTIVE_PAGE, sidebar_->currentPageId());

  // 侧边栏显隐状态（独立于 splitter 尺寸，避免 restoreState 布局时机问题）
  cfg.set(CONFIG_SIDEBAR_VISIBLE, sidebar_->isContentVisible());

  // 侧边栏展开宽度（会话间记忆）
  cfg.set(CONFIG_SIDEBAR_EXPANDED_WIDTH, sidebar_expanded_width_);

  // 底部面板状态（逐面板可见性 + 容器高度）
  int outIdx = bottom_container_->indexOf(log_panel_);
  int probIdx = bottom_container_->indexOf(problems_panel_);
  int termIdx = bottom_container_->indexOf(terminal_panel_);
  if (outIdx >= 0)
    cfg.set(CONFIG_BOTTOM_PANEL_LOG_VISIBLE,
            bottom_container_->isPanelVisible(outIdx));
  if (probIdx >= 0)
    cfg.set(CONFIG_BOTTOM_PANEL_PROBLEMS_VISIBLE,
            bottom_container_->isPanelVisible(probIdx));
  if (termIdx >= 0)
    cfg.set(CONFIG_BOTTOM_PANEL_TERMINAL_VISIBLE,
            bottom_container_->isPanelVisible(termIdx));
  cfg.set(CONFIG_BOTTOM_PANEL_HEIGHT, bottom_container_height_);

  // 辅助侧边栏状态（已由 h_splitter_->saveState() 保存）
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

  // 侧边栏显隐 — 使用显式配置（比 splitter 尺寸推断更可靠）
  bool sidebarVisible = cfg.get<bool>(CONFIG_SIDEBAR_VISIBLE, true);
  if (sidebarVisible) {
    sidebar_->showContent();
  } else {
    sidebar_->hideContent();
  }
  // 恢复侧边栏展开宽度
  sidebar_expanded_width_ = cfg.get<int>(CONFIG_SIDEBAR_EXPANDED_WIDTH, 280);

  // 注：侧边栏活动页、底部面板状态在 lazyInit 完成后恢复（部件尚不存在）

  // 辅助侧边栏：从 h_splitter_ restoreState 恢复的尺寸判断可见性
  auto hSizes = h_splitter_->sizes();
  bool auxVisible = hSizes.size() >= 3 && hSizes[2] > 0;
  if (auxVisible) {
    aux_sidebar_width_ = hSizes[2];
    aux_sidebar_widget_->show();
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
