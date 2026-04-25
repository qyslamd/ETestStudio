#include "MainWindow.h"

#include <QCloseEvent>
#include <QFileDialog>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>

#include "ActivityBarWidget.h"
#include "EditorManager.h"
#include "EditorWidget.h"
#include "FileExplorerWidget.h"
#include "OutputPanel.h"
#include "ProblemsPanel.h"
#include "SidebarWidget.h"
#include "TerminalPanel.h"
#include "config/ConfigDefs.h"
#include "config/ConfigManager.h"
#include "dialogs/NewProjectDialog.h"
#include "logger/Logger.h"
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

  createMenuBar();
  createStatusBar();

  // QADS Dock Manager
  dock_manager_ = new ads::CDockManager(this);

  // 中央编辑区（必须在添加其他dock之前建立）
  auto* centralPlaceholder = new QWidget(this);
  auto* centralDock = new ads::CDockWidget(QStringLiteral("中央编辑区"));
  centralDock->setObjectName("CentralDock"); // 设置唯一objectName
  centralDock->setWidget(centralPlaceholder);
  auto* centralArea = dock_manager_->setCentralWidget(centralDock);

  // 编辑器管理器
  editor_manager_ = new EditorManager(dock_manager_, this); // 不再需要传入centralArea

  // ==================== 左侧：活动栏 ====================
  activity_bar_ = new ActivityBarWidget(this);
  auto* activityDock = new ads::CDockWidget(QStringLiteral("活动栏"));
  activityDock->setObjectName("ActivityDock"); // 设置唯一objectName
  activityDock->setWidget(activity_bar_);
  activityDock->setFeature(ads::CDockWidget::DockWidgetClosable, false);
  activityDock->setFeature(ads::CDockWidget::DockWidgetFloatable, false);
  activityDock->setFeature(ads::CDockWidget::DockWidgetMovable, false);
  dock_manager_->addDockWidget(ads::LeftDockWidgetArea, activityDock);

  // ==================== 左侧：侧边栏 ====================
  sidebar_ = new SidebarWidget(this);
  auto* sidebarDock = new ads::CDockWidget(QStringLiteral("侧边栏"));
  sidebarDock->setObjectName("SidebarDock"); // 设置唯一objectName
  sidebarDock->setWidget(sidebar_);
  sidebarDock->setFeature(ads::CDockWidget::DockWidgetClosable, false);
  sidebarDock->setFeature(ads::CDockWidget::DockWidgetFloatable, false);
  sidebarDock->setFeature(ads::CDockWidget::DockWidgetMovable, false);
  dock_manager_->addDockWidget(ads::LeftDockWidgetArea, sidebarDock);

  // ==================== 底部：输出面板 ====================
  output_panel_ = new OutputPanel(this);
  auto* outputDock = new ads::CDockWidget(QStringLiteral("输出"));
  outputDock->setObjectName("OutputDock"); // 设置唯一objectName
  outputDock->setWidget(output_panel_);

  // ==================== 底部：问题面板 ====================
  problems_panel_ = new ProblemsPanel(this);
  auto* problemsDock = new ads::CDockWidget(QStringLiteral("问题"));
  problemsDock->setObjectName("ProblemsDock"); // 设置唯一objectName
  problemsDock->setWidget(problems_panel_);

  // ==================== 底部：终端面板 ====================
  terminal_panel_ = new TerminalPanel(this);
  auto* terminalDock = new ads::CDockWidget(QStringLiteral("终端"));
  terminalDock->setObjectName("TerminalDock"); // 设置唯一objectName
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
  propertyDock->setObjectName("PropertyDock"); // 设置唯一objectName
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
  connect(editor_manager_, &EditorManager::currentEditorChanged, this,
          [this](EditorWidget* editor) {
            bool hasEditor = (editor != nullptr);
            save_as_action_->setEnabled(hasEditor);
            close_file_action_->setEnabled(hasEditor);
            close_all_files_action_->setEnabled(hasEditor);

            // 保存按钮仅在当前文件有修改时启用
            save_action_->setEnabled(hasEditor && editor->isModified());

            if (editor) {
              statusBar()->showMessage(editor->filePath());
            } else {
              statusBar()->showMessage(QStringLiteral("就绪"));
            }
            updateWindowTitle();
          });

  // 编辑器：未保存更改状态变化时更新窗口标题和保存按钮状态
  connect(editor_manager_, &EditorManager::unsavedChangesChanged, this, [this]() {
    updateWindowTitle();
    // 有任何未保存的更改时启用保存所有按钮
    save_all_action_->setEnabled(editor_manager_->hasUnsavedChanges());
    // 更新当前文件的保存按钮状态
    auto* currentEditor = editor_manager_->currentEditor();
    save_action_->setEnabled(currentEditor != nullptr && currentEditor->isModified());
  });

  connect(
      editor_manager_, &EditorManager::fileOpened, this,
      [this](const QString&) { close_all_files_action_->setEnabled(true); });
  connect(editor_manager_, &EditorManager::fileClosed, this,
          [this](const QString&) {
            close_all_files_action_->setEnabled(
                editor_manager_->currentEditor() != nullptr);
          });
}

void MainWindow::createMenuBar() {
  auto* menuBar = this->menuBar();

  auto* fileMenu = menuBar->addMenu(QStringLiteral("文件(&F)"));
  fileMenu->addAction(QStringLiteral("新建项目"), this,
                      &MainWindow::onNewProject);
  fileMenu->addAction(QStringLiteral("打开项目"), this,
                      &MainWindow::onOpenProject);
  close_project_action_ = fileMenu->addAction(QStringLiteral("关闭项目"), this,
                                              &MainWindow::onCloseProject);
  close_project_action_->setEnabled(false);

  fileMenu->addSeparator();

  save_action_ = fileMenu->addAction(QStringLiteral("保存(&S)"), this,
                                     &MainWindow::onSaveFile);
  save_action_->setShortcut(QKeySequence::Save);
  save_action_->setEnabled(false);

  save_as_action_ = fileMenu->addAction(QStringLiteral("另存为..."), this,
                                        &MainWindow::onSaveFileAs);
  save_as_action_->setEnabled(false);

  save_all_action_ = fileMenu->addAction(QStringLiteral("保存所有(&L)"), this,
                                        &MainWindow::onSaveAllFiles);
  save_all_action_->setShortcut(QStringLiteral("Ctrl+Shift+S"));
  save_all_action_->setEnabled(false);

  fileMenu->addSeparator();

  close_file_action_ = fileMenu->addAction(QStringLiteral("关闭文件(&W)"), this,
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
  // 先尝试关闭当前项目
  if (!tryCloseCurrentProject()) {
    return; // 用户取消，不继续创建
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
    return; // 用户取消，不继续打开
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
        this, QStringLiteral("保存更改"),
        message,
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

    if (ret == QMessageBox::Cancel)
      return false;

    if (ret == QMessageBox::Yes) {
      // 只保存项目内的未保存文件
      if (!editor_manager_->saveModifiedFilesInDirectory(projectRoot)) {
        QMessageBox::warning(this, QStringLiteral("保存失败"),
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
  statusBar()->showMessage(QStringLiteral("项目已打开：%1").arg(projectPath));
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
          return; // 用户取消，不继续打开
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

}  // namespace etest::app
