#include "MainWindow.h"

#include <QCloseEvent>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>
#include <QTabWidget>

#include "ActivityBarWidget.h"
#include "OutputPanel.h"
#include "ProblemsPanel.h"
#include "SidebarWidget.h"
#include "TerminalPanel.h"
#include "config/ConfigDefs.h"
#include "config/ConfigManager.h"
#include "logger/Logger.h"

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
}

void MainWindow::createMenuBar() {
  auto* menuBar = this->menuBar();

  auto* fileMenu = menuBar->addMenu(QStringLiteral("文件(&F)"));
  fileMenu->addAction(QStringLiteral("新建项目"), this, []() {});
  fileMenu->addAction(QStringLiteral("打开项目"), this, []() {});
  fileMenu->addSeparator();
  fileMenu->addAction(QStringLiteral("退出"), this, &QWidget::close);

  menuBar->addMenu(QStringLiteral("编辑(&E)"));
  menuBar->addMenu(QStringLiteral("视图(&V)"));
  menuBar->addMenu(QStringLiteral("工具(&T)"));
  menuBar->addMenu(QStringLiteral("帮助(&H)"));
}

void MainWindow::createStatusBar() {
  statusBar()->showMessage(QStringLiteral("就绪"));
}

void MainWindow::closeEvent(QCloseEvent* event) {
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
