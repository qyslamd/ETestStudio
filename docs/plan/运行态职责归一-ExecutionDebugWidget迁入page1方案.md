# 运行态职责归一：ExecutionDebugWidget 迁入 page 1 方案

## 背景

ETestStudio 主窗口的中央区域采用 `QStackedWidget`（`central_stack_`）双页结构：

```
central_stack_ (QStackedWidget)
├── [0] page_editor   ← 编辑态：h_splitter_（侧边栏 | 编辑区 | 辅助侧边栏）
└── [1] page_exec     ← 运行态：ExecutionDashboard（独占整个中央区域）
```

侧边栏（`SidebarWidget`）位于 page 0 内部，是 `h_splitter_` 的一列。切到 page 1 后，page 0 整体从视图树移除，侧边栏不可见。

运行相关的 UI 组件当前分散在两处：

| 组件 | 位置 | 职责 | engine 信号接线 |
|------|------|------|-----------------|
| `ExecutionDebugWidget` | 侧边栏 kRun 页（page 0 内） | 运行前提检查 + suite/case/step 进度树 | 已接（`bindEngine()` 连 7 个信号） |
| `RunStatusPanel` | ExecutionDashboard 左列（page 1） | suite/case/step 进度树 + PASS/FAIL 统计 | **未接** |

## 问题

### 1. RunStatusPanel 是死代码

`ExecutionPanelController::connectEngineSignals()`（`ExecutionPanelController.cpp:178`）只调用了 `debug_widget_->bindEngine(engine_)`（line 219），从未将 engine 信号连接到 `RunStatusPanel`。`RunStatusPanel` 的 `onSuiteStarted / onCaseStarted / onStepFinished` 等方法无人调用。

`ExecutionDashboard::runStatusPanel()` getter 定义后也从未被外部调用（全仓库仅 1 处引用，即其自身声明）。page 1 左列的进度树始终为空。

### 2. 运行态体验割裂

用户点击「运行」时 `ExecutionPanelController::run()`（`ExecutionPanelController.cpp:323`）的执行顺序：

1. `debug_widget_->canRun()` 检查运行前提（line 327）
2. `sidebar_->switchPage(PageId::kRun)` 切侧边栏到 kRun 页（line 335）
3. `activity_bar_->setActivePageId(PageId::kRun)` 高亮活动栏（line 346）
4. `engine_->start()` 启动引擎（line 411）
5. `central_stack_->setCurrentIndex(1)` 切到 page 1（line 419）

步骤 2-3 切换的侧边栏位于 page 0，步骤 5 切到 page 1 后侧边栏不可见。实际效果：

- **page 1 可见**：ExecutionDashboard -- 左列是空的 RunStatusPanel，中列是 SignalTreePanel，右列是 VisualizationArea，底部是 ExecutionOutputPanel
- **page 0 不可见**：侧边栏停在 kRun 页，ExecutionDebugWidget 在此显示进度树 -- 用户看不到

用户若想看进度树，必须手动切回 page 0（编辑态），但此时编辑器仍然存在，体验混乱。

### 3. 功能重叠

`ExecutionDebugWidget` 和 `RunStatusPanel` 都实现 suite->case->step 的进度树，逻辑高度重复。区别在于：

- `ExecutionDebugWidget` 更完整：有运行前提检查（`canRun()` / `refreshPreconditions()` / `checkPreconditions()`）、`onStepStarted` 信号处理、步骤路径层级拆分（`findOrCreateStepItem` 支持 `a/b/c` 多级路径）
- `RunStatusPanel` 更简单：有底部统计标签（PASS/FAIL/TIMEOUT），但 `onStepFinished` 只接收字符串状态，无 `StepResult` 结构体

## 目标

**将运行态的全部 UI 职责归一到 page 1（ExecutionDashboard），消除 page 0 侧边栏中的运行相关组件。**

具体：

1. `ExecutionDebugWidget` 从侧边栏迁入 ExecutionDashboard 左列，替换 `RunStatusPanel`
2. 删除侧边栏 kRun 页和活动栏 kRun 按钮
3. 删除 `RunStatusPanel`（死代码，无外部引用）
4. `ExecutionPanelController` 不再操作侧边栏切页

## 改动清单

### 1. ExecutionDashboard -- 替换左列组件

| 文件 | 改动 |
|------|------|
| `src/app/ExecutionDashboard.h` | `RunStatusPanel* run_status_` -> `ExecutionDebugWidget* debug_widget_`；`runStatusPanel()` getter -> `debugWidget()` getter；前向声明 `RunStatusPanel` -> `ExecutionDebugWidget` |
| `src/app/ExecutionDashboard.cpp` | `#include "RunStatusPanel.h"` -> `#include "ExecutionDebugWidget.h"`；`initUi()` 中 `new RunStatusPanel` -> `new ExecutionDebugWidget` |

**ExecutionDashboard.h 改动**：

```cpp
// 改前
class RunStatusPanel;
// ...
RunStatusPanel* run_status_ = nullptr;
RunStatusPanel* runStatusPanel() const { return run_status_; }

// 改后
class ExecutionDebugWidget;
// ...
ExecutionDebugWidget* debug_widget_ = nullptr;
ExecutionDebugWidget* debugWidget() const { return debug_widget_; }
```

**ExecutionDashboard.cpp 改动**：

```cpp
// 改前
#include "RunStatusPanel.h"
// ...
run_status_ = new RunStatusPanel(main_splitter_);
run_status_->setObjectName(QStringLiteral("ExecRunStatus"));
// ...
main_splitter_->addWidget(run_status_);

// 改后
#include "ExecutionDebugWidget.h"
// ...
debug_widget_ = new ExecutionDebugWidget(main_splitter_);
debug_widget_->setObjectName(QStringLiteral("ExecRunStatus"));
// ...
main_splitter_->addWidget(debug_widget_);
```

### 2. ExecutionPanelController -- 改依赖来源

| 文件 | 改动 |
|------|------|
| `src/app/ExecutionPanelController.h` | `postInit()` 参数列表删除 `ExecutionDebugWidget* debug_widget`，同时删除失去用途的 `sidebar`/`h_splitter`/`activity_bar`/`sidebar_width_ref` 四个参数及对应成员（见下方说明）；`ExecutionDebugWidget* debug_widget_` 成员保留（来源从注入改为从 dashboard 取） |
| `src/app/ExecutionPanelController.cpp` | `postInit()` 实现删除 `debug_widget_` 及四个死成员赋值；`setDashboard()` 中 `debug_widget_ = dashboard_->debugWidget()`；`run()` / `runNextInQueue()` 删除侧边栏切换代码块 |

**postInit() 签名改动**：

删除侧边栏切换逻辑后，`sidebar_`/`h_splitter_`/`activity_bar_`/`sidebar_width_ref_` 四个成员除 `postInit()` 赋值外在 `run()`/`runNextInQueue()` 之外再无使用点，成为死代码。一并清理：

```cpp
// 改前
void postInit(ExecutionDebugWidget* debug_widget,
              ExecutionOutputPanel* output_panel,
              etest::core::SignalRegistry* signal_registry,
              std::shared_ptr<icd::Repository> icd_repository,
              EditorManager* editor_mgr,
              SidebarWidget* sidebar,
              QSplitter* h_splitter,
              ActivityBarWidget* activity_bar,
              TestProgramManagerWidget* test_program_mgr,
              ProblemsPanel* problems_panel,
              BottomContainerWidget* bottom_container,
              int* sidebar_width_ref,
              AppStatusBarController* status_bar_ctrl);

// 改后（删除 debug_widget + sidebar/h_splitter/activity_bar/sidebar_width_ref 共 5 个参数）
void postInit(ExecutionOutputPanel* output_panel,
              etest::core::SignalRegistry* signal_registry,
              std::shared_ptr<icd::Repository> icd_repository,
              EditorManager* editor_mgr,
              TestProgramManagerWidget* test_program_mgr,
              ProblemsPanel* problems_panel,
              BottomContainerWidget* bottom_container,
              AppStatusBarController* status_bar_ctrl);
```

对应删除 `ExecutionPanelController.h` 中的成员声明（`sidebar_`/`h_splitter_`/`activity_bar_`/`sidebar_width_ref_`）及前向声明（`SidebarWidget`/`ActivityBarWidget` 如无其他引用）。`ExecutionPanelController.cpp` 的 `postInit()` 实现同步删除这五行赋值。

**setDashboard() 中提取 debug_widget_**：

```cpp
void ExecutionPanelController::setDashboard(ExecutionDashboard* dashboard) {
  dashboard_ = dashboard;
  if (!dashboard_) {
    return;
  }
  debug_widget_ = dashboard_->debugWidget();  // 新增
  // ... 现有 SignalTreePanel / VisualizationArea 连接不变 ...
}
```

**run() 中删除侧边栏切换**（`ExecutionPanelController.cpp:333-347`）：

```cpp
// 删除整块：
// 1. 切换侧边栏
if (sidebar_) {
  sidebar_->switchPage(PageId::kRun);
  if (!sidebar_->isContentVisible() && h_splitter_) {
    sidebar_->showContent();
    auto sizes = h_splitter_->sizes();
    if (!sizes.isEmpty() && sidebar_width_ref_) {
      sizes[0] = *sidebar_width_ref_;
      h_splitter_->setSizes(sizes);
    }
  }
}
if (activity_bar_) {
  activity_bar_->setActivePageId(PageId::kRun);
}
```

**runNextInQueue() 中删除同样代码**（`ExecutionPanelController.cpp:603-617`）：逻辑与 run() 完全相同，一并删除。

> `connectEngineSignals()` 中 `debug_widget_->bindEngine(engine_)`（line 218-220）无需改动，指针来源变了但调用不变。

### 3. MainWindow::lazyInit() -- 去掉侧边栏 kRun 页

| 文件 | 改动 |
|------|------|
| `src/app/MainWindow.cpp` | 删除 `activity_bar_->addPage(kRun, ...)`；删除 `ExecutionDebugWidget` 创建 + `sidebar_->addPage(kRun, ...)`；`postInit()` 调用去掉首参 |
| `src/app/MainWindow.h` | 删除 `ExecutionDebugWidget* execution_debug_widget_` 成员；删除 `ExecutionDebugWidget.h` include（如无其他引用） |

**lazyInit() 删除内容**（`MainWindow.cpp:1008-1009`）：

```cpp
// 删除：
activity_bar_->addPage(PageId::kRun, QStringLiteral("运行"),
                       QStringLiteral("debug"));
```

**lazyInit() 删除内容**（`MainWindow.cpp:1029-1031`）：

```cpp
// 删除：
auto* runPanel = new ExecutionDebugWidget(sidebar_);
sidebar_->addPage(PageId::kRun, runPanel, QStringLiteral("执行调试"));
execution_debug_widget_ = runPanel;
```

**postInit() 调用改动**（`MainWindow.cpp:1088-1092`）：

```cpp
// 改前
execution_controller_->postInit(
    execution_debug_widget_, execution_output_panel_, nullptr, nullptr,
    editor_manager_, sidebar_, h_splitter_, activity_bar_, test_program_mgr_,
    problems_panel_, bottom_container_, &sidebar_expanded_width_,
    status_bar_ctrl_);

// 改后（删除 execution_debug_widget_ + sidebar_/h_splitter_/activity_bar_/&sidebar_expanded_width_ 共 5 个参数）
execution_controller_->postInit(
    execution_output_panel_, nullptr, nullptr,
    editor_manager_, test_program_mgr_,
    problems_panel_, bottom_container_, status_bar_ctrl_);
```

**MainWindow.h 改动**：

```cpp
// 删除：
ExecutionDebugWidget* execution_debug_widget_ = nullptr;
```

### 4. 删除 RunStatusPanel

| 文件 | 操作 |
|------|------|
| `src/app/RunStatusPanel.h` | 删除 |
| `src/app/RunStatusPanel.cpp` | 删除 |
| `src/app/CMakeLists.txt` | 删除 `RunStatusPanel.h` 和 `RunStatusPanel.cpp` 两行（line 117-118） |

### 5. 清理 PageId::kRun 常量

`PageId::kRun`（`SidebarWidget.h:27`）删除后无任何引用，可一并删除：

| 文件 | 改动 |
|------|------|
| `src/app/SidebarWidget.h` | 删除 `constexpr auto kRun = "run";` |

### 6. ExecutionDebugWidget 进度树补 objectName + QSS

`RunStatusPanel` 的进度树设有 `objectName("RunStatusTree")`，QSS 中有 `#RunStatusTree { background: transparent; }` 规则（`vscode.qss:2054`、`default.qss:1135`）。而 `ExecutionDebugWidget::initUi()` 的 `tree_progress_`（`.cpp:48`）**未设 objectName**，全仓库 QSS 中也无任何 `ExecutionDebugWidget`/`debugOverview`/`overviewSummary` 相关规则。迁入 dashboard 后进度树将使用默认背景，与 dashboard 深色区域不协调。

需在 `ExecutionDebugWidget::initUi()` 中为 `tree_progress_` 补 objectName：

| 文件 | 改动 |
|------|------|
| `src/app/ExecutionDebugWidget.cpp` | `tree_progress_` 创建后追加 `tree_progress_->setObjectName(QStringLiteral("ExecDebugTree"));` |

### 7. 清理 QSS 死样式 + 补新规则

删除 `RunStatusPanel` 后，`#RunStatusTree` 和 `#RunStatusStats` 两段样式成为死代码。同时为新的 `#ExecDebugTree` 补透明背景规则，保持与原 `RunStatusPanel` 视觉一致：

| 文件 | 改动 |
|------|------|
| `src/app/resources/styles/vscode.qss` | 删除 `#RunStatusTree`（line 2054-2056）和 `#RunStatusStats`（line 2057-2061）两段；新增 `#ExecDebugTree { background: transparent; }` |
| `src/app/resources/styles/default.qss` | 删除 `#RunStatusTree`（line 1135-1137）和 `#RunStatusStats`（line 1138-1141）两段；新增 `#ExecDebugTree { background: transparent; }` |

**vscode.qss 改动**：

```css
/* 改前 */
/* RunStatusPanel */
#RunStatusTree {
    background: transparent;
}
#RunStatusStats {
    padding: 4px;
    font-size: 11px;
    color: #CCCCCC;
}

/* 改后 */
/* ExecutionDebugWidget */
#ExecDebugTree {
    background: transparent;
}
```

**default.qss 改动**：

```css
/* 改前 */
/* RunStatusPanel */
#RunStatusTree {
    background: transparent;
}
#RunStatusStats {
    padding: 4px;
    font-size: 11px;
}

/* 改后 */
/* ExecutionDebugWidget */
#ExecDebugTree {
    background: transparent;
}
```

## 改动后的运行态架构

```
用户点击「运行」
    │
    ▼
ExecutionPanelController::run()
    ├── debug_widget_->canRun()          ← 前提检查（从 dashboard 取指针）
    ├── engine_->start()                 ← 启动引擎
    └── central_stack_->setCurrentIndex(1)  ← 切到 page 1
                │
                ▼
        ExecutionDashboard (page 1)
        ├── 左列：ExecutionDebugWidget
        │   ├── 运行前提概览（canRun 检查结果）
        │   └── 进度树（suite -> case -> step，实时更新）
        ├── 中列：SignalTreePanel（信号监控勾选）
        ├── 右列：VisualizationArea（信号波形可视化）
        └── 底部：ExecutionOutputPanel（执行日志输出）
```

侧边栏不再有 kRun 页，活动栏不再有「运行」按钮。运行态的一切都在 page 1。

## 涉及文件汇总

| 文件 | 改动类型 |
|------|----------|
| `src/app/ExecutionDashboard.h` | 修改：替换成员类型 + getter |
| `src/app/ExecutionDashboard.cpp` | 修改：替换 include + initUi 组件创建 + addWidget |
| `src/app/ExecutionPanelController.h` | 修改：postInit 签名（删 5 参数）+ 删 4 个死成员 |
| `src/app/ExecutionPanelController.cpp` | 修改：postInit 实现 + setDashboard + run + runNextInQueue |
| `src/app/MainWindow.h` | 修改：删除成员变量 |
| `src/app/MainWindow.cpp` | 修改：lazyInit 删除 kRun 页 + postInit 调用 |
| `src/app/ExecutionDebugWidget.cpp` | 修改：tree_progress_ 补 setObjectName |
| `src/app/SidebarWidget.h` | 修改：删除 kRun 常量 |
| `src/app/RunStatusPanel.h` | 删除 |
| `src/app/RunStatusPanel.cpp` | 删除 |
| `src/app/CMakeLists.txt` | 修改：删除 RunStatusPanel 两行 |
| `src/app/resources/styles/vscode.qss` | 修改：删除 RunStatusPanel 死样式 + 新增 ExecDebugTree 规则 |
| `src/app/resources/styles/default.qss` | 修改：删除 RunStatusPanel 死样式 + 新增 ExecDebugTree 规则 |

## 风险与回滚

### 风险

- **风险低**：`RunStatusPanel` 无外部调用，删除安全；`ExecutionDebugWidget` 接口不变，只是父窗口从 sidebar 换成 dashboard 的 splitter
- **parent 变更**：从 `SidebarWidget` 改为 `QSplitter`（dashboard 内）。经核查 `ExecutionDebugWidget` 及其子控件均无依赖父级样式表的 QSS 规则，事件传递不受 parent 影响
- **时序安全**：`debug_widget_` 有两条使用路径，均安全：
  - **`connectEngineSignals()` 路径**：`setDashboard()`（`MainWindow.cpp:1094`）在 `lazyInit()` 中执行并赋值 `debug_widget_`；`createEngine()` -> `connectEngineSignals()` -> `debug_widget_->bindEngine()` 仅在 `run()`（line 377）和 `runNextInQueue()`（line 623）中触发，均为用户操作，在 lazyInit 完成之后
  - **`verify()` 路径**：`verify()`（line 449）中 `debug_widget_->setDependencies()`（line 553）同样为用户触发，此时 `debug_widget_` 已由 `setDashboard()` 赋值就绪
- **残留配置**：用户上次停留在 kRun 页时，`CONFIG_SIDEBAR_ACTIVE_PAGE` 持久化值为 `"run"`。删除 kRun 后 `sidebar_->pageById("run")` 返回 nullptr（`SidebarWidget.cpp:100-106`），`lazyInit` 中的 `if (sidebarVisible && sidebar_->pageById(activePage))` 条件不成立，侧边栏静默回退到 `addPage` 自动选中的默认页（kProjectOverview），不崩溃但无用户可见反馈。可在 `lazyInit` 恢复侧边栏页面前增加对已删除 page id 的清理逻辑，或接受静默回退（影响极小）

### 回滚

恢复 `RunStatusPanel` 文件 + CMakeLists + QSS + 还原各处改动即可
