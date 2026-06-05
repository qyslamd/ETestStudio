# MainWindow 懒加载 + 脉冲覆盖层 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 MainWindow 改为两阶段初始化，先显示窗口框架（带脉冲覆盖层），再懒加载子控件。

**Architecture:** 构造函数拆为 initUi() + QTimer::singleShot(0) → lazyInit()。initUi() 只创建窗口骨架，lazyInit() 创建所有子控件并通过覆盖层掩护。信号拆为 initSignalsEarly()（Phase 1 依赖）和 initSignalsLate()（跨组件信号）。

**Tech Stack:** Qt 5.15 / C++17 / QTimer / QPainter

---

### Task 1: 创建 LoadingOverlay 覆盖层

**Files:**
- Create: `src/app/LoadingOverlay.h`
- Create: `src/app/LoadingOverlay.cpp`

**LoadingOverlay 设计：**
- 继承 QWidget，parent = v_splitter_（内容区）
- 全透明背景（`setAttribute(Qt::WA_TranslucentBackground)`）
- 脉冲光圈动效：QTimer 每 30ms 触发，pulse_alpha_ 在 160~220 正弦呼吸
- 居中绘制脉冲光圈 + "正在加载..." 文字
- eventFilter 拦截鼠标/滚轮/按键事件
- finish() 方法用 QTimer 递减动画淡出，结束后 deleteLater
- 10 秒超时自动移除

- [ ] **Step 1: 创建 LoadingOverlay.h**

```cpp
#pragma once

#include <QTimer>
#include <QWidget>

namespace etest::app {

/// 主窗口懒加载期间的脉冲覆盖层
/// 盖在 v_splitter_ 内容区上，Ribbon 和活动栏保持可见
class LoadingOverlay : public QWidget {
    Q_OBJECT
public:
    explicit LoadingOverlay(QWidget* parent);
    ~LoadingOverlay() override;

    /// 开始脉冲动效并设置 10 秒超时
    void startWithTimeout(int timeoutMs = 10000);

    /// 淡出动画，结束后 deleteLater
    void finish();

signals:
    void finished();

protected:
    void paintEvent(QPaintEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void onPulseTick();
    void onFadeTick();

    int pulse_alpha_ = 180;          // 当前脉冲透明度
    double pulse_phase_ = 0.0;       // 正弦相位
    QTimer* pulse_timer_ = nullptr;   // 脉冲驱动

    int fade_alpha_ = 255;           // 淡出递减
    QTimer* fade_timer_ = nullptr;    // 淡出驱动
    QTimer* timeout_timer_ = nullptr; // 超时保护
};

}  // namespace etest::app
```

- [ ] **Step 2: 创建 LoadingOverlay.cpp**

```cpp
#include "LoadingOverlay.h"

#include <QPainter>
#include <QResizeEvent>
#include <QtMath>

#include "ThemeManager.h"

namespace etest::app {

LoadingOverlay::LoadingOverlay(QWidget* parent)
    : QWidget(parent) {
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setMouseTracking(true);
    raise();

    // 脉冲定时器
    pulse_timer_ = new QTimer(this);
    connect(pulse_timer_, &QTimer::timeout, this, &LoadingOverlay::onPulseTick);

    // 淡出定时器
    fade_timer_ = new QTimer(this);
    connect(fade_timer_, &QTimer::timeout, this, &LoadingOverlay::onFadeTick);
    fade_timer_->setInterval(16);  // ~60fps

    // 超时保护
    timeout_timer_ = new QTimer(this);
    timeout_timer_->setSingleShot(true);
    connect(timeout_timer_, &QTimer::timeout, this, [this]() {
        finish();
    });

    // 安装事件过滤器拦截交互
    if (parent) {
        parent->installEventFilter(this);
        // 同步父窗口大小
        setGeometry(parent->rect());
    }
}

LoadingOverlay::~LoadingOverlay() {
    if (parent()) {
        parent()->removeEventFilter(this);
    }
}

void LoadingOverlay::startWithTimeout(int timeoutMs) {
    pulse_phase_ = 0.0;
    pulse_alpha_ = 180;
    pulse_timer_->start(30);
    show();
    raise();
    timeout_timer_->start(timeoutMs);
}

void LoadingOverlay::finish() {
    pulse_timer_->stop();
    timeout_timer_->stop();
    fade_alpha_ = 255;
    fade_timer_->start();
}

void LoadingOverlay::onPulseTick() {
    // 正弦呼吸：相位 0→2π 循环
    pulse_phase_ += 0.15;
    if (pulse_phase_ > 6.2832)
        pulse_phase_ -= 6.2832;
    double val = std::sin(pulse_phase_);
    pulse_alpha_ = 190 + static_cast<int>(val * 30);
    update();
}

void LoadingOverlay::onFadeTick() {
    fade_alpha_ -= 17;
    if (fade_alpha_ <= 0) {
        fade_alpha_ = 0;
        fade_timer_->stop();
        hide();
        emit finished();
        deleteLater();
        return;
    }
    update();
}

void LoadingOverlay::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    bool dark = ThemeManager::instance().isDarkTheme();

    // 当前透明度 = 脉冲值（正常态）或 fade 值（淡出态）
    int alpha = fade_timer_->isActive() ? fade_alpha_ : pulse_alpha_;

    QColor bg = dark ? QColor(30, 30, 46) : QColor(245, 245, 245);
    bg.setAlpha(alpha);
    p.fillRect(rect(), bg);

    // 脉冲光圈（绘制一个半透明圆环）
    if (!fade_timer_->isActive() || fade_alpha_ > 60) {
        QColor circleColor = dark ? QColor(100, 140, 255) : QColor(0, 120, 215);
        circleColor.setAlpha(pulse_alpha_);

        QPointF center = rect().center();
        qreal radius = 30.0;
        // 外圈
        p.setBrush(Qt::NoBrush);
        QPen pen(circleColor, 3);
        p.setPen(pen);
        p.drawEllipse(center, radius, radius);

        // 内圈（透明度更高）
        circleColor.setAlpha(pulse_alpha_ - 60);
        pen.setColor(circleColor);
        pen.setWidth(2);
        p.setPen(pen);
        p.drawEllipse(center, radius * 0.6, radius * 0.6);

        // "正在加载..." 文字
        QColor textColor = dark ? Qt::white : Qt::black;
        textColor.setAlpha(
            fade_timer_->isActive()
                ? qMin(fade_alpha_, 255)
                : 255);
        p.setPen(textColor);
        QFont font = p.font();
        font.setPointSize(11);
        p.setFont(font);
        QRect textRect(0, (int)center.y() + 50, width(), 30);
        p.drawText(textRect, Qt::AlignCenter, QStringLiteral("正在加载..."));
    }
}

bool LoadingOverlay::eventFilter(QObject* obj, QEvent* event) {
    if (isVisible()) {
        switch (event->type()) {
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
        case QEvent::MouseButtonDblClick:
        case QEvent::MouseMove:
        case QEvent::Wheel:
        case QEvent::KeyPress:
        case QEvent::KeyRelease:
            return true;  // 拦截
        default:
            break;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void LoadingOverlay::resizeEvent(QResizeEvent*) {
    // 父窗口 resize 时自动同步（由 installEventFilter 处理）
}

}  // namespace etest::app
```

- [ ] **Step 3: 更新 CMakeLists.txt — 添加 LoadingOverlay 文件**

Edit `src/app/CMakeLists.txt`，在 `widgets/TuxSaverOverlay.cpp` 后添加：

```cmake
    widgets/TuxSaverOverlay.h
    widgets/TuxSaverOverlay.cpp
    LoadingOverlay.h             # ← 新增
    LoadingOverlay.cpp            # ← 新增
```

- [ ] **Step 4: 构建验证**

```bash
scripts/build_ninja.bat
```
Expected: 编译通过，无警告。

- [ ] **Step 5: 提交**

```bash
git add -A
git commit -m "feat: add LoadingOverlay widget with pulse animation"
```

---

### Task 2: 修改 main_window.h — 新增方法声明

**Files:**
- Modify: `src/app/main_window.h`

- [ ] **Step 1: 添加 LoadingOverlay 前置声明**

在 `class HintBarWidget;` 后添加：
```cpp
class LoadingOverlay;
```

- [ ] **Step 2: 添加新方法声明**

在 `void initSignals();` 后添加：
```cpp
  void initSignalsEarly();
  void initSignalsLate();
  void lazyInit();
```

- [ ] **Step 3: 添加 LoadingOverlay 成员变量**

在 TuxSaverOverlay 成员附近添加：
```cpp
  LoadingOverlay* loading_overlay_ = nullptr;
```

- [ ] **Step 4: 提交**

```bash
git commit -m "feat(mainwindow): add lazy loading method declarations"
```

---

### Task 3: 拆分 MainWindow::initUi() → Phase 1 + lazyInit()

**Files:**
- Modify: `src/app/main_window.cpp`

核心思路：initUi() 只保留窗口框架创建，其余子控件移到 lazyInit()。

- [ ] **Step 1: 修改 MainWindow 构造函数**

原构造函数：
```cpp
MainWindow::MainWindow(...) {
  initUi();
  initSignals();
  AuthService::instance();
  updateWindowTitle();
  // 插件加载...
  // 硬件刷新...
  // Tux 屏保...
}
```

改为：
```cpp
MainWindow::MainWindow(...) {
  initUi();
  initSignalsEarly();
  // 安排懒加载（窗口 show() 之后执行）
  QTimer::singleShot(0, this, &MainWindow::lazyInit);
}
```

移除构造函数中原来的 AuthService、插件加载、硬件刷新、TuxSaverOverlay 创建。

- [ ] **Step 2: 精简 initUi() — 删除侧边栏页面注册**

initUi() 中删除以下代码块（约 197-248 行）：

```cpp
  // ── 注册侧边栏页面（按活动栏顺序） ──
  // 项目概览 → 后续替换为 ProjectStructureWidget
  sidebar_->addPage(PageId::kProjectOverview,
                    new ProjectStructureWidget(sidebar_),
                    QStringLiteral("项目概览"));
  // 拓扑 → 占位，待 TopologyManagerWidget 实现
  sidebar_->addPage(PageId::kTopology, new QWidget(sidebar_),
                    QStringLiteral("拓扑"));
  // ... 共 9 个 addPage 调用
```

- [ ] **Step 3: 精简 initUi() — 删除活动栏按钮注册**

删除：
```cpp
  activity_bar_->addPage(PageId::kProjectOverview, ...);
  // ... 共 9 个 addPage 调用
```

- [ ] **Step 4: 精简 initUi() — 删除 WelcomeWidget / EditorManager / 底部面板创建**

initUi() 替换 WelcomeWidget 和 EditorManager 为占位 widget：

删除：
```cpp
  // 中央编辑区：Welcome页面
  welcome_widget_ = new WelcomeWidget(this);
  central_dock_ = new ads::CDockWidget(QStringLiteral("欢迎"));
  central_dock_->setObjectName("CentralDock");
  central_dock_->setWidget(welcome_widget_);
  ...
  editor_manager_ = new EditorManager(dock_manager_, this);
```

替换为：
```cpp
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
```

删除底部面板创建：
```cpp
  output_panel_ = new OutputPanel(this);
  problems_panel_ = new ProblemsPanel(this);
  terminal_panel_ = new TerminalPanel(this);
  bottom_container_ = new BottomContainerWidget(v_splitter_);
  bottom_container_->addPanel(...);
  v_splitter_->addWidget(bottom_container_);
```

替换为：
```cpp
  // 底部容器空壳（lazyInit 时 addPanel）
  bottom_container_ = new BottomContainerWidget(v_splitter_);
  v_splitter_->addWidget(bottom_container_);
  bottom_container_->hide();  // 无面板时隐藏
```

- [ ] **Step 5: 删除 initUi() 中的 hint_bar_ 占位消息**

删除：
```cpp
  hint_bar_->postHint(QStringLiteral("已打开项目「测试项目」"));
  // ... 共 5 行
```

保留 hint_bar_ 的创建（Phase 1 需要它），消息发布移到 lazyInit。

- [ ] **Step 6: 调整 initUi() 中的 splitter sizes**

由于 Phase 1 没有 WelcomeWidget 和底部面板，splitter 初始大小改为留出空间给 QADS 占位：
```cpp
  h_splitter_->setSizes({280, 800, 0});   // 不变
  v_splitter_->setSizes({800, 0});         // 底部面板初始大小为 0（后续恢复）
```

- [ ] **Step 7: 编写 lazyInit() 方法**

在 `initUi()` 定义之后或 `initSignals()` 之前添加：

```cpp
void MainWindow::lazyInit() {
    // 1. 创建 LoadingOverlay 盖住内容区，启动脉冲
    loading_overlay_ = new LoadingOverlay(v_splitter_);
    loading_overlay_->startWithTimeout(10000);
    QCoreApplication::processEvents();  // 立即渲染覆盖层

    // 2. 注册活动栏按钮
    activity_bar_->addPage(PageId::kProjectOverview, QStringLiteral("项目概览"),
                           QStringLiteral("project"));
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

    // 3. 注册侧边栏页面
    sidebar_->addPage(PageId::kProjectOverview,
                      new ProjectStructureWidget(sidebar_),
                      QStringLiteral("项目概览"));
    sidebar_->addPage(PageId::kTopology, new QWidget(sidebar_),
                        QStringLiteral("拓扑"));
    sidebar_->addPage(PageId::kHardware, new HardwareTreeWidget(sidebar_),
                        QStringLiteral("硬件"));
    sidebar_->addPage(PageId::kProtocol, new ProtocolManagerWidget(sidebar_),
                        QStringLiteral("协议"));
    sidebar_->addPage(PageId::kTestProgram,
                      new TestProgramManagerWidget(sidebar_),
                      QStringLiteral("用例"));
    sidebar_->addPage(PageId::kRun, new QWidget(sidebar_),
                        QStringLiteral("运行"));
    sidebar_->addPage(PageId::kReport, new QWidget(sidebar_),
                        QStringLiteral("报告"));
    sidebar_->addPage(PageId::kSearch, new SearchWidget(sidebar_),
                        QStringLiteral("搜索"));
    sidebar_->addPage(PageId::kGit, new GitWidget(sidebar_),
                        QStringLiteral("Git"));

    // 4. 创建底部面板
    output_panel_ = new OutputPanel(this);
    problems_panel_ = new ProblemsPanel(this);
    terminal_panel_ = new TerminalPanel(this);
    bottom_container_->addPanel(QStringLiteral("输出"), output_panel_);
    bottom_container_->addPanel(QStringLiteral("问题"), problems_panel_);
    bottom_container_->addPanel(QStringLiteral("终端"), terminal_panel_);
    if (!bottom_container_->isVisible()) {
        bottom_container_->show();
        v_splitter_->setSizes({600, 200});
    }

    // 5. 创建 EditorManager
    editor_manager_ = new EditorManager(dock_manager_, this);

    // 6. 创建 WelcomeWidget 替换中央占位
    welcome_widget_ = new WelcomeWidget(this);
    central_dock_->setWidget(welcome_widget_);
    welcome_widget_->refreshRecentProjects();

    // 7. 连接跨组件信号（此时所有子控件已就绪）
    initSignalsLate();

    // 8. 初始化认证服务
    AuthService::instance();
    updateWindowTitle();

    // 9. 加载插件并刷新硬件树
    auto& pluginMgr = etest::core::plugin::PluginManager::instance();
    pluginMgr.addSearchPath(QCoreApplication::applicationDirPath() + "/plugins");
    pluginMgr.loadAll();
    sidebar_->hardwareTree()->refreshTree();

    // 10. 发布提示消息
    hint_bar_->postHint(QStringLiteral("已打开项目「测试项目」"));
    hint_bar_->postHint(QStringLiteral("编译完成，发现 2 个警告"));
    hint_bar_->postHint(QStringLiteral("有新版本可用，请更新"),
                        QStringLiteral("更新"), [] { /* 占位 */ });
    hint_bar_->postHint(QStringLiteral("文件「test_spec.xml」已自动保存"),
                        QStringLiteral("查看"), [] { /* 占位 */ });
    hint_bar_->postHint(QStringLiteral("远程连接已断开，尝试重连中..."),
                        QStringLiteral("重试"), [] { /* 占位 */ });

    // 11. 移除覆盖层（淡出）
    connect(loading_overlay_, &LoadingOverlay::finished, this, [this]() {
        LOG_INFO("MAIN", "懒加载完成");
    });
    loading_overlay_->finish();

    // 12. Tux 屏保（独立创建，不影响主流程）
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

    LOG_INFO("MAIN", "mainwindow 懒加载完成");
}
```

- [ ] **Step 8: 提交**

```bash
git commit -m "refactor(mainwindow): split initUi into Phase 1 + lazyInit"
```

---

### Task 4: 拆分 initSignals() → initSignalsEarly() + initSignalsLate()

**Files:**
- Modify: `src/app/main_window.cpp`

- [ ] **Step 1: 编写 initSignalsEarly()**

从原来的 `initSignals()` 中提取不依赖子控件的信号，连接到 Phase 1 已有控件：

```cpp
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
            settings_dialog_->setStyleSheet(styleSheet());
            connect(settings_dialog_, &QDialog::finished, this,
                    [this]() { activity_bar_->setSettingsActive(false); });
        }
        activity_bar_->setSettingsActive(true);
        settings_dialog_->show();
        settings_dialog_->raise();
        settings_dialog_->activateWindow();
    });

    // 活动栏：页面切换（Phase 2 才实际切换页面，但信号先连上）
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
```

- [ ] **Step 2: 编写 initSignalsLate()**

从原来的 `initSignals()` 提取所有依赖子控件的信号（注意很多信号通过 `sidebar_->xxx()` 访问子控件，lazyInit 中页面已注册，take 可用）：

```cpp
void MainWindow::initSignalsLate() {
    // 项目管理信号
    auto& projectMgr = etest::core::project::ProjectManager::instance();
    connect(&projectMgr, &ProjectManager::projectCreated, this,
            &MainWindow::onProjectOpened);
    connect(&projectMgr, &ProjectManager::projectOpened, this,
            &MainWindow::onProjectOpened);
    connect(&projectMgr, &ProjectManager::projectClosed, this,
            &MainWindow::onProjectClosed);
    connect(&projectMgr, &ProjectManager::recentProjectsChanged, this,
            &MainWindow::updateRecentProjectsMenu);

    // 脏检查回调
    projectMgr.setDirtyCheckCallback(
        [this]() { return editor_manager_->hasUnsavedChanges(); });

    // 项目结构树 ↔ ProjectManager
    auto* psWidget = qobject_cast<ProjectStructureWidget*>(
        sidebar_->pageById(PageId::kProjectOverview));
    if (psWidget) {
        connect(&projectMgr, &ProjectManager::projectOpened, psWidget,
                &ProjectStructureWidget::setProjectPath);
        connect(&projectMgr, &ProjectManager::projectClosed, psWidget,
                &ProjectStructureWidget::clearProjectPath);
        connect(psWidget, &ProjectStructureWidget::fileOpenRequested, psWidget,
                [this](const QString& path) { editor_manager_->openFile(path); });
        connect(psWidget, &ProjectStructureWidget::fileOpenAsTextRequested, psWidget,
                [this](const QString& path) {
                    editor_manager_->openFile(path, QStringLiteral("text"));
                });
        connect(psWidget, &ProjectStructureWidget::fileDeleted, editor_manager_,
                &EditorManager::onFileDeleted);
        connect(psWidget, &ProjectStructureWidget::fileRenamed, editor_manager_,
                &EditorManager::onFileRenamed);
    }

    // 搜索组件
    auto* searchWidget = sidebar_->searchWidget();
    connect(&projectMgr, &ProjectManager::projectOpened, searchWidget,
            [searchWidget](const QString& projectPath) {
                searchWidget->setSearchRoot(projectPath);
            });
    connect(&projectMgr, &ProjectManager::projectClosed, searchWidget,
            [searchWidget]() { searchWidget->setSearchRoot({}); });
    connect(searchWidget, &SearchWidget::fileOpenRequested, editor_manager_,
            &EditorManager::openFileAtLine);

    // Git 面板
    auto* gitWidget = sidebar_->gitWidget();
    connect(&projectMgr, &ProjectManager::projectOpened, gitWidget,
            [gitWidget](const QString& projectPath) {
                gitWidget->setProjectRoot(projectPath);
            });
    connect(&projectMgr, &ProjectManager::projectClosed, gitWidget,
            [gitWidget]() { gitWidget->setProjectRoot({}); });
    connect(gitWidget, &GitWidget::fileOpenRequested, gitWidget,
            [this](const QString& path) { editor_manager_->openFile(path); });

    // 备份管理
    auto& backupMgr = etest::core::backup::BackupManager::instance();
    connect(&projectMgr, &ProjectManager::projectOpened, &backupMgr,
            [&backupMgr](const QString& projectPath) {
                backupMgr.onProjectOpened(projectPath);
            });
    connect(&projectMgr, &ProjectManager::projectClosed, &backupMgr,
            [&backupMgr]() { backupMgr.onProjectClosed(); });

    // 欢迎页
    connect(welcome_widget_, &WelcomeWidget::newProjectRequested, this,
            &MainWindow::onNewProject);
    connect(welcome_widget_, &WelcomeWidget::openProjectRequested, this,
            &MainWindow::onOpenProject);
    connect(welcome_widget_, &WelcomeWidget::projectOpenRequested, this,
            &MainWindow::openRecentProject);
    connect(&projectMgr, &ProjectManager::recentProjectsChanged, welcome_widget_,
            &WelcomeWidget::refreshRecentProjects);

    // 编辑器 ↔ 状态栏 / Ribbon 操作
    connect(editor_manager_, &EditorManager::currentEditorChanged, this,
            [this](IEditor* editor) {
                // ... 整个 lambda 从原 initSignals() 复制过来，完整保留
            });
    connect(editor_manager_, &EditorManager::unsavedChangesChanged, this,
            [this]() {
                updateWindowTitle();
                save_all_action_->setEnabled(editor_manager_->hasUnsavedChanges());
            });
    connect(editor_manager_, &EditorManager::modificationChanged, this,
            [this](bool modified) { save_action_->setEnabled(modified); });
    connect(editor_manager_, &EditorManager::fileOpened, this,
            [this](const QString&) { close_all_files_action_->setEnabled(true); });
    connect(editor_manager_, &EditorManager::fileClosed, this,
            [this](const QString&) {
                close_all_files_action_->setEnabled(
                    editor_manager_->currentEditor() != nullptr);
            });

    // 编辑器初始状态
    IEditor* current_editor = editor_manager_->currentEditor();
    bool hasEditor = (current_editor != nullptr);
    bool hasSelection = false;
    if (hasEditor) {
        if (auto* textEditor = dynamic_cast<TextEditorWidget*>(current_editor)) {
            hasSelection = textEditor->editor()->hasSelectedText();
        }
    }
    save_action_->setEnabled(hasEditor && current_editor->isModified());
    edit_undo_action_->setEnabled(hasEditor);
    edit_redo_action_->setEnabled(hasEditor);
    edit_cut_action_->setEnabled(hasSelection);
    edit_copy_action_->setEnabled(hasSelection);
    edit_paste_action_->setEnabled(hasEditor);

    // 剪贴板
    clipboard_ = QGuiApplication::clipboard();
    auto updatePasteState = [this]() {
        bool hasEd = (editor_manager_->currentEditor() != nullptr);
        bool hasText = !clipboard_->text().isEmpty();
        edit_paste_action_->setEnabled(hasEd && hasText);
    };
    connect(clipboard_, &QClipboard::dataChanged, this, updatePasteState);
    updatePasteState();

    // Ctrl+B 切换侧边栏
    auto* toggleSidebar = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_B), this);
    connect(toggleSidebar, &QShortcut::activated, this, [this]() {
        // ... 复制原逻辑
    });

    // Ctrl+Shift+F 全局搜索
    auto* globalSearchShortcut =
        new QShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_F), this);
    connect(globalSearchShortcut, &QShortcut::activated, this, [this]() {
        // ... 复制原逻辑
    });

    // 硬件树：插件加载/卸载时自动刷新
    auto* hardwareTree = sidebar_->hardwareTree();
    auto& pluginMgr = etest::core::plugin::PluginManager::instance();
    connect(&pluginMgr, &etest::core::plugin::PluginManager::pluginLoaded,
            hardwareTree, &HardwareTreeWidget::refreshTree);
    connect(&pluginMgr, &etest::core::plugin::PluginManager::pluginUnloaded,
            hardwareTree, &HardwareTreeWidget::refreshTree);

    // 协议管理器
    auto* protocolMgr = sidebar_->protocolManager();
    connect(protocolMgr, &ProtocolManagerWidget::openFileRequested, protocolMgr,
            [this](const QString& path) { editor_manager_->openFile(path); });
    connect(&projectMgr, &ProjectManager::projectOpened, protocolMgr,
            &ProtocolManagerWidget::refreshList);
    connect(&projectMgr, &ProjectManager::projectClosed, protocolMgr,
            &ProtocolManagerWidget::refreshList);

    // 用例管理器
    auto* tpMgr = sidebar_->testProgramManager();
    connect(tpMgr, &TestProgramManagerWidget::openFileRequested, tpMgr,
            [this](const QString& path) { editor_manager_->openFile(path); });
    connect(&projectMgr, &ProjectManager::projectOpened, tpMgr,
            &TestProgramManagerWidget::refreshList);
    connect(&projectMgr, &ProjectManager::projectClosed, tpMgr,
            &TestProgramManagerWidget::refreshList);

    // 日志输出到界面
    auto* qtSink = etest::core::logger::Logger::qtConsoleSink();
    if (qtSink) {
        connect(qtSink, &QtConsoleSink::logMessage, output_panel_,
                &OutputPanel::appendLog);
    }

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
                        sizes[2] = 0;
                        h_splitter_->setSizes(sizes);
                    }
                    aux_sidebar_widget_->hide();
                }
            });

    // 认证信号
    connect(&AuthService::instance(), &AuthService::loginSucceeded, this,
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
```

- [ ] **Step 3: 删除旧 initSignals() 方法**

将原 `initSignals()` 方法体替换为空函数或直接删除，因为它的内容已被拆到 early 和 late。

- [ ] **Step 4: 构建验证**

```bash
scripts/build_ninja.bat
```
Expected: 编译通过。如果 sidebar_->xxx() 访问发生在 lazyInit 之后，确保无空指针解引用。

- [ ] **Step 5: 提交**

```bash
git commit -m "refactor(mainwindow): split initSignals into early and late"
```

---

### Task 5: 最终集成验证

**Files:** 无代码改动

- [ ] **Step 1: 完整构建**

```bash
scripts/build_ninja.bat
```
Expected: 0 errors, 0 warnings.

- [ ] **Step 2: 逻辑审查**

grep 检查是否还有任何对 `welcome_widget_`、`editor_manager_`、`sidebar_->pageById()`、`sidebar_->hardwareTree()` 的调用出现在 `lazyInit()` 之前：

```bash
# 确认所有 sidebar_ 页面访问发生在 lazyInit 之后
grep -n "sidebar_->" src/app/main_window.cpp | head -40
```

- [ ] **Step 3: 确认构造函数不再有阻塞操作**

构造函数应该只有：
1. `initUi()` — Phase 1 框架
2. `initSignalsEarly()` — 轻量信号
3. `QTimer::singleShot(0, this, &MainWindow::lazyInit)` — 触发懒加载

- [ ] **Step 4: 最终提交**

```bash
git add -A
git commit -m "feat(mainwindow): implement lazy loading with loading overlay"
```
