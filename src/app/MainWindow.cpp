#include "MainWindow.h"

#include <QCloseEvent>
#include <QFileDialog>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QTabWidget>

#include "ActivityBarWidget.h"
#include "FileExplorerWidget.h"
#include "OutputPanel.h"
#include "ProblemsPanel.h"
#include "SidebarWidget.h"
#include "TerminalPanel.h"
#include "dialogs/NewProjectDialog.h"
#include "config/ConfigDefs.h"
#include "config/ConfigManager.h"
#include "logger/Logger.h"
#include "project/ProjectManager.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      dock_manager_(nullptr),
      activity_bar_(nullptr),
      sidebar_(nullptr),
      output_panel_(nullptr),
      problems_panel_(nullptr),
      terminal_panel_(nullptr) {
  initUi();
  initSignals();
  updateWindowTitle();
  LOG_INFO("MAIN", "主窗口初始化完成");
}

MainWindow::~MainWindow() = default;

void MainWindow::initUi() {
  setWindowTitle("ETest Demo");
  setMinimumSize(900, 600);

  createMenuBar();
  createStatusBar();

  // QADS Dock Manager
  dock_manager_ = new ads::CDockManager(this);

  // ==================== 中央编辑器占位 ====================
  auto* centralPlaceholder = new QLabel(QStringLiteral("编辑器区域"), this);
  centralPlaceholder->setAlignment(Qt::AlignCenter);
  auto* centralDock = new ads::CDockWidget(QStringLiteral("编辑器"));
  centralDock->setWidget(centralPlaceholder);
  centralDock->setFeature(ads::CDockWidget::DockWidgetClosable, false);
  centralDock->setFeature(ads::CDockWidget::DockWidgetMovable, false);
  centralDock->setFeature(ads::CDockWidget::DockWidgetFloatable, false);
  dock_manager_->setCentralWidget(centralDock);

  // ==================== 左侧：活动栏 ====================
  activity_bar_ = new ActivityBarWidget(this);
  auto* activityDock = new ads::CDockWidget(QStringLiteral("活动栏"));
  activityDock->setWidget(activity_bar_);
  activityDock->setFeature(ads::CDockWidget::DockWidgetClosable, false);
  activityDock->setFeature(ads::CDockWidget::DockWidgetFloatable, false);
  activityDock->setFeature(ads::CDockWidget::DockWidgetMovable, false);
  dock_manager_->addDockWidget(ads::LeftDockWidgetArea, activityDock);

  // ==================== 左侧：侧边栏 ====================
  sidebar_ = new SidebarWidget(this);
  auto* sidebarDock = new ads::CDockWidget(QStringLiteral("侧边栏"));
  sidebarDock->setWidget(sidebar_);
  sidebarDock->setFeature(ads::CDockWidget::DockWidgetClosable, false);
  sidebarDock->setFeature(ads::CDockWidget::DockWidgetFloatable, false);
  sidebarDock->setFeature(ads::CDockWidget::DockWidgetMovable, false);
  dock_manager_->addDockWidget(ads::LeftDockWidgetArea, sidebarDock);

  // ==================== 底部：输出面板 ====================
  output_panel_ = new OutputPanel(this);
  auto* outputDock = new ads::CDockWidget(QStringLiteral("输出"));
  outputDock->setWidget(output_panel_);

  // ==================== 底部：问题面板 ====================
  problems_panel_ = new ProblemsPanel(this);
  auto* problemsDock = new ads::CDockWidget(QStringLiteral("问题"));
  problemsDock->setWidget(problems_panel_);

  // ==================== 底部：终端面板 ====================
  terminal_panel_ = new TerminalPanel(this);
  auto* terminalDock = new ads::CDockWidget(QStringLiteral("终端"));
  terminalDock->setWidget(terminal_panel_);

  // 底部面板区域：输出面板为主，问题和终端tab到输出面板
  dock_manager_->addDockWidget(ads::BottomDockWidgetArea, outputDock);
  problemsDock->setFeature(ads::CDockWidget::DockWidgetFloatable, true);
  dock_manager_->addDockWidgetTab(ads::BottomDockWidgetArea, problemsDock);
  terminalDock->setFeature(ads::CDockWidget::DockWidgetFloatable, true);
  dock_manager_->addDockWidgetTab(ads::BottomDockWidgetArea, terminalDock);

  // ==================== 右侧：属性面板占位 ====================
  auto* propertyPlaceholder = new QLabel(QStringLiteral("属性面板"), this);
  propertyPlaceholder->setAlignment(Qt::AlignCenter);
  auto* propertyDock = new ads::CDockWidget(QStringLiteral("属性"));
  propertyDock->setWidget(propertyPlaceholder);
  dock_manager_->addDockWidget(ads::RightDockWidgetArea, propertyDock);

  // 恢复窗口状态
  restoreWindowState();
}

void MainWindow::initSignals() {
  // 活动栏切换侧边栏
  connect(activity_bar_, &ActivityBarWidget::activityClicked, sidebar_,
          &SidebarWidget::switchPage);

  // 项目管理信号
  auto& projectMgr = etest::core::project::ProjectManager::instance();
  connect(&projectMgr, &etest::core::project::ProjectManager::projectOpened,
          this, &MainWindow::onProjectOpened);
  connect(&projectMgr, &etest::core::project::ProjectManager::projectClosed,
          this, &MainWindow::onProjectClosed);
  connect(&projectMgr,
          &etest::core::project::ProjectManager::recentProjectsChanged, this,
          &MainWindow::updateRecentProjectsMenu);

  // 文件浏览器：项目打开/关闭时设置根路径
  auto* fileExplorer = sidebar_->fileExplorer();
  connect(&projectMgr, &etest::core::project::ProjectManager::projectOpened,
          fileExplorer, [fileExplorer](const QString& projectPath) {
            fileExplorer->setRootPath(projectPath);
          });
  connect(&projectMgr, &etest::core::project::ProjectManager::projectClosed,
          fileExplorer, [fileExplorer]() { fileExplorer->setRootPath({}); });

  // 文件浏览器：双击文件打开（当前阶段仅日志，编辑器模块对接后创建标签页）
  connect(fileExplorer, &FileExplorerWidget::fileOpenRequested, this,
          [](const QString& filePath) {
            LOG_INFO("MAIN", "请求打开文件：{}", filePath.toStdString());
          });
}

void MainWindow::createMenuBar() {
  auto* menuBar = this->menuBar();

  auto* fileMenu = menuBar->addMenu(QStringLiteral("文件(&F)"));
  fileMenu->addAction(QStringLiteral("新建项目"), this,
                       &MainWindow::onNewProject);
  fileMenu->addAction(QStringLiteral("打开项目"), this,
                       &MainWindow::onOpenProject);
  close_project_action_ =
      fileMenu->addAction(QStringLiteral("关闭项目"), this,
                           &MainWindow::onCloseProject);
  close_project_action_->setEnabled(false);

  fileMenu->addSeparator();

  recent_projects_menu_ =
      fileMenu->addMenu(QStringLiteral("最近项目"));
  updateRecentProjectsMenu();

  fileMenu->addSeparator();
  fileMenu->addAction(QStringLiteral("退出"), this, &QWidget::close);

  menuBar->addMenu(QStringLiteral("编辑(&E)"));
  menuBar->addMenu(QStringLiteral("视图(&V)"));
  menuBar->addMenu(QStringLiteral("工具(&T)"));
  menuBar->addMenu(QStringLiteral("帮助(&H)"));
}

void MainWindow::createStatusBar() {
  status_project_label_ = new QLabel(this);
  status_project_label_->setText(QStringLiteral("无打开项目"));
  statusBar()->addPermanentWidget(status_project_label_);
  statusBar()->showMessage(QStringLiteral("就绪"));
}

void MainWindow::onNewProject() {
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
  auto& cfg = ConfigManager::instance();
  QString lastPath = cfg.get<QString>(CONFIG_RECENT_LAST_OPEN_PATH);
  if (lastPath.isEmpty()) {
    lastPath = QDir::homePath();
  }

  QString filePath = QFileDialog::getOpenFileName(
      this, QStringLiteral("打开项目"), lastPath,
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

void MainWindow::onCloseProject() {
  auto& projectMgr = etest::core::project::ProjectManager::instance();
  if (!projectMgr.isProjectOpen()) return;

  if (projectMgr.hasUnsavedChanges()) {
    int ret = QMessageBox::question(
        this, QStringLiteral("保存更改"),
        QStringLiteral("项目有未保存的更改，是否保存？"),
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

    if (ret == QMessageBox::Cancel) return;
    if (ret == QMessageBox::Yes) {
      projectMgr.saveProject();
    }
  }

  projectMgr.closeProject();
}

void MainWindow::onProjectOpened(const QString& projectPath) {
  close_project_action_->setEnabled(true);

  auto& projectMgr = etest::core::project::ProjectManager::instance();
  auto* project = projectMgr.currentProject();
  if (project) {
    status_project_label_->setText(project->name());
  }

  updateWindowTitle();
  statusBar()->showMessage(
      QStringLiteral("项目已打开：%1").arg(projectPath));
}

void MainWindow::onProjectClosed() {
  close_project_action_->setEnabled(false);
  status_project_label_->setText(QStringLiteral("无打开项目"));
  updateWindowTitle();
  statusBar()->showMessage(QStringLiteral("项目已关闭"));
}

void MainWindow::updateWindowTitle() {
  auto& projectMgr = etest::core::project::ProjectManager::instance();
  if (projectMgr.isProjectOpen()) {
    auto* project = projectMgr.currentProject();
    setWindowTitle(
        QStringLiteral("%1 - ETest Demo").arg(project->name()));
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
          path, this, [this, path]() {
            auto& pm =
                etest::core::project::ProjectManager::instance();
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

void MainWindow::closeEvent(QCloseEvent* event) {
  auto& projectMgr = etest::core::project::ProjectManager::instance();
  if (projectMgr.isProjectOpen()) {
    if (projectMgr.hasUnsavedChanges()) {
      int ret = QMessageBox::question(
          this, QStringLiteral("保存更改"),
          QStringLiteral("项目有未保存的更改，是否保存？"),
          QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

      if (ret == QMessageBox::Cancel) {
        event->ignore();
        return;
      }
      if (ret == QMessageBox::Yes) {
        projectMgr.saveProject();
      }
    }
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
}
