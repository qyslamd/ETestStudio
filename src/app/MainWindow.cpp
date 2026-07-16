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
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QProcess>
#include <QScrollBar>
#include <QShortcut>
#include <QSplitter>
#include <QStackedWidget>
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
#include "AppStatusBarController.h"
#include "EditorManager.h"
#include "EditorPanelController.h"
#include "ExecutionDebugWidget.h"
#include "ExecutionPanelController.h"
#include "GitWidget.h"
#include "HardwareTreeWidget.h"
#include "ProjectController.h"
#include "ProjectStructureWidget.h"
#include "ProtocolManagerWidget.h"
#include "SearchWidget.h"
#include "SidebarWidget.h"
#include "SignalRegistry.h"
#include "TerminalPanel.h"
#include "TestProgramData.h"
#include "TestProgramEditorWidget.h"
#include "TestProgramManagerWidget.h"
#include "ThemeManager.h"
#include "TopologyManagerWidget.h"
#include "TuxSaverController.h"
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
#include "widgets/ExecutionOutputPanel.h"
#include "widgets/HintBarWidget.h"
#include "widgets/LoadingOverlay.h"
#include "widgets/LogOutputPanel.h"
#include "widgets/ProblemsPanel.h"

using namespace etest::core::config;
using namespace etest::core::project;
using namespace etest::core::logger;
using namespace etest::core::auth;

namespace etest::app {

using etest::core_ui::AppIconProvider;
using etest::core_ui::ThemeManager;

MainWindow::MainWindow(QWidget* parent)
    : SARibbonMainWindow(parent),
      status_bar_ctrl_(new AppStatusBarController(this)),
      execution_controller_(new ExecutionPanelController(this, this)) {
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
      if (tux_controller_) {
        tux_controller_->onUserActivity();
      }
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

  // ==================== 中央堆叠容器（编辑态/运行态） ====================
  central_stack_ = new QStackedWidget(this);
  central_stack_->setObjectName("centralStack");

  // ── 页 0：编辑态（活动栏 + 水平分割器 + 底部面板） ──
  page_editor_widget_ = new QWidget(central_stack_);
  page_editor_widget_->setObjectName("PageEditor");
  auto* page0_layout = new QHBoxLayout(page_editor_widget_);
  page0_layout->setContentsMargins(0, 0, 0, 0);
  page0_layout->setSpacing(0);

  // ==================== 活动栏 ====================
  activity_bar_ = new ActivityBarWidget(page_editor_widget_);
  page0_layout->addWidget(activity_bar_);

  // ==================== 水平分割器 ====================
  h_splitter_ = new QSplitter(Qt::Horizontal, page_editor_widget_);
  h_splitter_->setChildrenCollapsible(true);
  page0_layout->addWidget(h_splitter_, 1);

  // ===== 侧边栏 =====
  sidebar_ = new SidebarWidget(h_splitter_);
  h_splitter_->addWidget(sidebar_);

  // ===== 垂直分割器（编辑器区域 + 底部面板） =====
  v_splitter_ = new QSplitter(Qt::Vertical, h_splitter_);
  v_splitter_->setChildrenCollapsible(true);
  h_splitter_->addWidget(v_splitter_);

  // 编辑器区域（提示栏 + DockManager）
  auto* editor_area = new QWidget(v_splitter_);
  editor_area->setObjectName("EditorArea");
  auto* editor_area_layout = new QVBoxLayout(editor_area);
  editor_area_layout->setContentsMargins(0, 0, 0, 0);
  editor_area_layout->setSpacing(0);

  hint_bar_ = new HintBarWidget(editor_area);
  editor_area_layout->addWidget(hint_bar_);

  ads::CDockManager::setConfigFlag(ads::CDockManager::AlwaysShowTabs, true);
  ads::CDockManager::setConfigFlag(
      ads::CDockManager::MiddleMouseButtonClosesTab, true);
  ads::CDockManager::setConfigFlag(ads::CDockManager::AllTabsHaveCloseButton,
                                   true);
  dock_manager_ = new ads::CDockManager(editor_area);
  editor_area_layout->addWidget(dock_manager_, 1);

  // 中央占位（lazyInit 时替换为 WelcomeWidget）
  auto* placeholder = new QWidget(editor_area);
  placeholder->setObjectName("CentralPlaceholder");
  central_dock_ = new ads::CDockWidget(QStringLiteral("欢迎"));
  central_dock_->setObjectName("CentralDock");
  central_dock_->setWidget(placeholder);
  central_dock_->tabWidget()->setElideMode(Qt::ElideNone);
  dock_manager_->setCentralWidget(central_dock_);
  central_dock_->setFeature(ads::CDockWidget::DockWidgetClosable, true);
  auto* centralDockArea = central_dock_->dockAreaWidget();
  if (centralDockArea) {
    setupDockTitleBarButtons(centralDockArea);
  }

  v_splitter_->addWidget(editor_area);

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

  central_stack_->addWidget(page_editor_widget_);  // index 0

  // ── 页 1：运行态（占位，后续替换为执行仪表盘） ──
  exec_dashboard_page_ = new QWidget(central_stack_);
  exec_dashboard_page_->setObjectName("ExecDashboardPage");
  auto* exec_layout = new QVBoxLayout(exec_dashboard_page_);
  auto* exec_placeholder =
      new QLabel(QStringLiteral("执行仪表盘（待实现）"), exec_dashboard_page_);
  exec_placeholder->setAlignment(Qt::AlignCenter);
  exec_layout->addWidget(exec_placeholder);
  central_stack_->addWidget(exec_dashboard_page_);  // index 1

  central_stack_->setCurrentIndex(0);  // 默认编辑态

  setCentralWidget(central_stack_);

  // 设置 splitter 初始尺寸
  h_splitter_->setSizes({280, 800, 0});  // sidebar / 垂直区域 / aux
  v_splitter_->setSizes({800, 0});       // 底部面板初始大小为 0（后续恢复）

  // 追踪底部面板高度变化（用户拖动 splitter 手柄时实时更新）
  connect(v_splitter_, &QSplitter::splitterMoved, this, [this](int pos, int idx) {
    auto s = v_splitter_->sizes();
    if (s.size() >= 2) {
      bottom_container_height_ = s[1];
    }
  });

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
      if (sizes.size() >= 2) {
        int total = sizes[0] + sizes[1];
        int targetBottom = qMin(bottom_container_height_, total);
        sizes[0] = total - targetBottom;
        sizes[1] = targetBottom;
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
          status_bar_ctrl_->setMessage(editor->filePath());

          if (auto* textEditor = dynamic_cast<TextEditorWidget*>(editor)) {
            QsciScintilla* sci_editor = textEditor->editor();
            hasSelection = sci_editor->hasSelectedText();

            int line, col;
            sci_editor->getCursorPosition(&line, &col);
            status_bar_ctrl_->setCursorPos(line + 1, col + 1);
            status_bar_ctrl_->setLanguage(QStringLiteral("纯文本"));
            status_bar_ctrl_->setEol(QStringLiteral("CRLF"));
            status_bar_ctrl_->setEncoding(QStringLiteral("UTF-8"));

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
            status_bar_ctrl_->setCursorPos(1, 1);
            status_bar_ctrl_->setLanguage(QStringLiteral("纯文本"));
            status_bar_ctrl_->setEol(QStringLiteral("CRLF"));
            status_bar_ctrl_->setEncoding(QStringLiteral("UTF-8"));
            edit_undo_action_->setEnabled(false);
            edit_redo_action_->setEnabled(false);
          }
        } else {
          status_bar_ctrl_->setMessage(QStringLiteral("就绪"));
          status_bar_ctrl_->setCursorPos(1, 1);
          status_bar_ctrl_->setLanguage(QStringLiteral("纯文本"));
          status_bar_ctrl_->setEol(QStringLiteral("CRLF"));
          status_bar_ctrl_->setEncoding(QStringLiteral("UTF-8"));
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
                  [](const QString&) {});
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

  // 创建 ProjectController 和 EditorPanelController（依赖 editor_manager_）
  project_controller_ = new ProjectController(this, editor_manager_, this);
  editor_controller_ =
      new EditorPanelController(editor_manager_, status_bar_ctrl_, this);

  // 连接 project_controller_ 信号 → MainWindow 槽
  connect(project_controller_, &ProjectController::fileRequested, this,
          [this](const QString& path) {
            // 与 MainWindow::onOpenFile 处理逻辑一致
            auto& cfg = ConfigManager::instance();
            cfg.set(CONFIG_RECENT_LAST_OPEN_PATH,
                    QFileInfo(path).absolutePath());
            editor_manager_->openFile(path);
          });

  // 执行控制器依赖注入（signal_registry_/icd_repository_ 项目打开时才可用）
  execution_controller_->postInit(
      execution_debug_widget_, execution_output_panel_, nullptr, nullptr,
      editor_manager_, sidebar_, h_splitter_, activity_bar_, test_program_mgr_,
      problems_panel_, bottom_container_, &sidebar_expanded_width_,
      status_bar_ctrl_);
  execution_controller_->setCentralStack(central_stack_);

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

  // 9. 发布提示消息（DEBUG模式打开）
#ifdef _DEBUG
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
#endif

  // 10. 恢复底部面板状态（逐面板可见性 + 容器显隐）
  step_timer.restart();
  {
    auto& cfg = ConfigManager::instance();
    // 恢复水平 splitter（布局已就绪）
    QByteArray hState = cfg.get<QByteArray>(CONFIG_WINDOW_H_SPLITTER_STATE);
    if (!hState.isEmpty()) {
      h_splitter_->restoreState(hState);
    }
    // 辅助侧边栏：从恢复后的 h_splitter 尺寸推断可见性
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

    // 恢复垂直 splitter
    QByteArray vState = cfg.get<QByteArray>(CONFIG_WINDOW_V_SPLITTER_STATE);
    if (!vState.isEmpty()) {
      v_splitter_->restoreState(vState);
    }
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
      if (vSizes.size() >= 2) {
        int total = vSizes[0] + vSizes[1];
        int targetBottom = qMin(bottom_container_height_, total);
        vSizes[0] = total - targetBottom;
        vSizes[1] = targetBottom;
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

  // 12. Tux 屏保（委托给 TuxSaverController）
  step_timer.restart();
  tux_controller_ = new TuxSaverController(this, this);
  tux_controller_->start();
  qApp->installEventFilter(this);  // 全局事件捕获，用于重置屏保空闲计时器
  LOG_INFO("LAZY", "  [12/12] Tux 屏保: {} ms", step_timer.elapsed());

  connect(&ConfigManager::instance(), &ConfigManager::configChanged, this,
          [this](const QString& key) {
            if (key == QString::fromLatin1(CONFIG_TUXSAVER_ENABLED) &&
                !ConfigManager::instance().get<bool>(
                    CONFIG_TUXSAVER_ENABLED, CONFIG_TUXSAVER_DEFAULT_ENABLED)) {
              if (tux_controller_) {
                tux_controller_->onUserActivity();
              }
            }
          });

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

    // 更新下拉菜单按钮图标适配主题
    auto* centralArea = central_dock_->dockAreaWidget();
    if (centralArea) {
      auto* tabMenuBtn =
          centralArea->titleBar()->findChild<QToolButton*>("tabsMenuButton");
      if (tabMenuBtn) {
        tabMenuBtn->setIcon(
            AppIconProvider::instance().icon("drop_down_arrow"));
      }
    }
  }
}

void MainWindow::createStatusBar() {
  status_bar_ctrl_->setup(statusBar());
}

void MainWindow::onNewProject() {
  project_controller_->newProject();
}

void MainWindow::onOpenProject() {
  project_controller_->openProject();
}

void MainWindow::onOpenFile() {
  project_controller_->openFile();
}

QString MainWindow::findProjectFile(const QString& dirPath) {
  return ProjectController::findProjectFile(dirPath);
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
  project_controller_->closeProject();
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
    status_bar_ctrl_->setProject(project->name());
  }

  updateWindowTitle();
  status_bar_ctrl_->setMessage(
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

  // 将 ICD 上下文同步到 ExecutionPanelController
  execution_controller_->updateIcdContext(signal_registry_, icd_repository_);

  // 确保引擎就绪（此时 signal_registry_ 和 icd_repository_ 已可用）
  execution_controller_->createEngine();

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
              id, name, dobj[QStringLiteral("type")].toString());
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
                [](const QString&) {});
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
  status_bar_ctrl_->setProject(QStringLiteral("无打开项目"));
  updateWindowTitle();
  status_bar_ctrl_->setMessage(QStringLiteral("项目已关闭"));

  // M6: 清理 ICD 上下文
  execution_controller_->destroyEngine();

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
  editor_controller_->saveCurrent();
}

void MainWindow::onSaveFileAs() {
  editor_controller_->saveCurrentAs();
}

void MainWindow::onSaveAllFiles() {
  editor_controller_->saveAll();
}

void MainWindow::onCloseCurrentFile() {
  editor_controller_->closeCurrent();
}

void MainWindow::onCloseAllFiles() {
  editor_controller_->closeAll();
}

void MainWindow::onUndo() {
  editor_controller_->undo();
}

void MainWindow::onRedo() {
  editor_controller_->redo();
}

void MainWindow::onCut() {
  editor_controller_->cut();
}

void MainWindow::onCopy() {
  editor_controller_->copy();
}

void MainWindow::onPaste() {
  editor_controller_->paste();
}

void MainWindow::onFind() {
  editor_controller_->find();
}

void MainWindow::onReplace() {
  editor_controller_->replace();
}

void MainWindow::onGoToLine() {
  editor_controller_->goToLine();
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

// ── Ribbon 运行按钮 ──

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
  if (tux_controller_) {
    tux_controller_->stop();
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
  connect(edit_undo_action_, &QAction::triggered, this, &MainWindow::onUndo);

  edit_redo_action_ =
      new QAction(style()->standardIcon(QStyle::SP_ArrowForward),
                  QStringLiteral("重做"), this);
  edit_redo_action_->setShortcut(QKeySequence::Redo);
  edit_redo_action_->setEnabled(false);
  connect(edit_redo_action_, &QAction::triggered, this, &MainWindow::onRedo);

  edit_cut_action_ = new QAction(QStringLiteral("剪切"), this);
  edit_cut_action_->setShortcut(QKeySequence::Cut);
  edit_cut_action_->setEnabled(false);
  connect(edit_cut_action_, &QAction::triggered, this, &MainWindow::onCut);

  edit_copy_action_ = new QAction(QStringLiteral("复制"), this);
  edit_copy_action_->setShortcut(QKeySequence::Copy);
  edit_copy_action_->setEnabled(false);
  connect(edit_copy_action_, &QAction::triggered, this, &MainWindow::onCopy);

  edit_paste_action_ = new QAction(QStringLiteral("粘贴"), this);
  edit_paste_action_->setShortcut(QKeySequence::Paste);
  edit_paste_action_->setEnabled(false);
  connect(edit_paste_action_, &QAction::triggered, this, &MainWindow::onPaste);

  edit_find_action_ = new QAction(QStringLiteral("查找"), this);
  edit_find_action_->setShortcut(QKeySequence::Find);
  edit_find_action_->setEnabled(false);
  connect(edit_find_action_, &QAction::triggered, this, &MainWindow::onFind);

  edit_replace_action_ = new QAction(QStringLiteral("替换"), this);
  edit_replace_action_->setShortcut(QKeySequence::Replace);
  edit_replace_action_->setEnabled(false);
  connect(edit_replace_action_, &QAction::triggered, this,
          &MainWindow::onReplace);

  edit_go_to_line_action_ = new QAction(QStringLiteral("跳转到行"), this);
  edit_go_to_line_action_->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_G));
  edit_go_to_line_action_->setEnabled(false);
  connect(edit_go_to_line_action_, &QAction::triggered, this,
          &MainWindow::onGoToLine);

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
  connect(logoutAction, &QAction::triggered, this, [this]() {
    LOG_INFO("MAIN_UI", "点击「退出登录」");
    AuthService::instance().logout();
  });
  connect(logoutAction, &QAction::triggered, this, [this]() {
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

    // 执行控制 Panel（QAction 由 ExecutionPanelController 管理）
    auto* panel_control = cat->addPanel(QStringLiteral("执行控制"));
    execution_controller_->runAction()->setIcon(
        AppIconProvider::instance().icon(QStringLiteral("run")));
    execution_controller_->pauseAction()->setIcon(
        AppIconProvider::instance().icon(QStringLiteral("pause")));
    execution_controller_->stopAction()->setIcon(
        AppIconProvider::instance().icon(QStringLiteral("stop")));
    execution_controller_->verifyAction()->setIcon(
        AppIconProvider::instance().icon(QStringLiteral("verify")));
    panel_control->addLargeAction(execution_controller_->verifyAction());
    panel_control->addLargeAction(execution_controller_->runAction());
    panel_control->addSmallAction(execution_controller_->pauseAction());
    panel_control->addSmallAction(execution_controller_->stopAction());

    connect(execution_controller_->runAction(), &QAction::triggered, this,
            [this]() { execution_controller_->run(); });
    connect(execution_controller_->pauseAction(), &QAction::triggered, this,
            [this]() { execution_controller_->pause(); });
    connect(execution_controller_->stopAction(), &QAction::triggered, this,
            [this]() { execution_controller_->stop(); });
    connect(execution_controller_->verifyAction(), &QAction::triggered, this,
            [this]() { execution_controller_->verify(); });

    // 运行方式 Panel
    auto* panel_mode = cat->addPanel(QStringLiteral("运行方式"));
    execution_controller_->runAllAction()->setIcon(
        AppIconProvider::instance().icon(QStringLiteral("run_all")));
    panel_mode->addLargeAction(execution_controller_->runAllAction());

    connect(execution_controller_->runAllAction(), &QAction::triggered, this,
            [this]() { execution_controller_->runAll(); });

    // 统计 Panel
    auto* panel_stats = cat->addPanel(QStringLiteral("统计"));
    panel_stats->addSmallWidget(execution_controller_->ribbonStatsLabel());
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

  // 替换 Ribbon 最小化/还原按钮图标为自定义 SVG
  if (auto* minAction = ribbon->minimumModeAction()) {
    auto updateMinIcon = [this, ribbon, minAction]() {
      minAction->setIcon(AppIconProvider::instance().icon(
          ribbon->isMinimumMode() ? QStringLiteral("ribbon_expand")
                                  : QStringLiteral("ribbon_collapse")));
    };

    // 断开 SARibbonBar 自带的 triggered 图标覆盖逻辑
    disconnect(minAction, &QAction::triggered, nullptr, nullptr);

    // 自己的 triggered：切换模式 + 换图标（一次搞定）
    connect(minAction, &QAction::triggered, this,
            [this, ribbon, updateMinIcon]() {
              ribbon->setMinimumMode(!ribbon->isMinimumMode());
              updateMinIcon();
            });

    // 响应外部触发的模式变化（如双击 tab）
    connect(ribbon, &SARibbonBar::ribbonModeChanged, this,
            [updateMinIcon](SARibbonBar::RibbonMode) { updateMinIcon(); });

    updateMinIcon();
  }

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
  execution_controller_->syncControlStates();
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
  // 水平 splitter 和辅助侧边栏也在 lazyInit 中恢复（布局就绪后）
}

void MainWindow::setupDockTitleBarButtons(ads::CDockAreaWidget* area) {
  if (!area)
    return;
  auto* titleBar = area->titleBar();
  if (!titleBar)
    return;
  for (auto* btn : titleBar->findChildren<QToolButton*>()) {
    auto name = btn->objectName();
    // 备注：
    // 分离:detachGroupButton
    // 关闭:dockAreaCloseButton
    // 下拉菜单:tabsMenuButton
    if (name == "detachGroupButton" || name == "dockAreaCloseButton") {
      btn->hide();
    }
  }
}

}  // namespace etest::app
