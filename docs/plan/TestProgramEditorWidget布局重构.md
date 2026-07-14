# TestProgramEditorWidget 布局重构

> 目标：移除 `ExecutionControlBar`，运行控制归入 Ribbon，执行统计归入 MainWindow StatusBar。
> 嵌入模式与独立模式均不往编辑器内加运行按钮。

---

## 问题概述

`TestProgramEditorWidget` 当前布局：

```
┌──────────────────────────────────────────────────┐
│ QToolBar                                          │
│  撤销 重做 │ 用例+/- │ 步骤+/- ↑↓ │ spacer │ 纵向标签 │
├──────────────────────────────────────────────────┤
│ QTabWidget (初始化 / 用例1 / 用例2 / ... / 清理)    │
├──────────────────────────────────────────────────┤
│ ExecutionControlBar (底部)                        │
│  ▶ 运行  ⏸ 暂停  ⏹ 停止  │  ✅ 0  ❌ 0  ⏱ 0s │ 就绪 │
└──────────────────────────────────────────────────┘
```

问题：
1. 运行控制栏在底部，步骤表格滚动时不可见
2. 嵌入 `etest_app` 时，顶层 MainWindow 已有 Ribbon 运行按钮 + QStatusBar，底部控制栏完全冗余
3. 独立模式 (`src/tools/test-program-editor`) 是纯编辑器，只负责编辑 `.etprog` 文件，不需要运行控制
4. `ExecutionControlBar` 的 `updateStats()` 从未被调用，stats 从未生效

---

## 改动方案

### `ExecutionControlBar` 拆解

| 原职责 | 去向 | 说明 |
|--------|------|------|
| ▶⏸⏹ 按钮 | **删除** | Ribbon 已有 `act_run_/act_pause_/act_stop_`，由 `syncControlStates()` 管理 |
| ✅❌⏱ 统计 | → MainWindow QStatusBar | 新增 `label_exec_stats_` permanent QLabel，由引擎事件驱动更新 |
| "就绪/运行中" 状态文字 | → MainWindow QStatusBar | 由 `engineStateChanged` 信号驱动 `showMessage()` |
| `runClicked/pauseClicked/stopClicked` 信号 | **删除** | 现有 Ribbon action 的 `triggered` 直接连接 `onRunClicked` 等 |

### 目标布局

```
┌───────────────────────────────────────────────────────────┐
│ SARibbon (MainWindow)                                      │
│  ┌─────┐ ┌──────┐ ┌──────┐   ┌──────────────────────────┐ │
│  │ ▶运行│ │ ⏸暂停│ │ ⏹停止│   │ 其他 Ribbon 页签          │ │
│  └─────┘ └──────┘ └──────┘   └──────────────────────────┘ │
├───────────────────────────────────────────────────────────┤
│ QToolBar (TestProgramEditorWidget)                         │
│  撤销 重做 │ 用例+/- │ 步骤+/- ↑↓ │ spacer │ 纵向标签       │
├───────────────────────────────────────────────────────────┤
│ QTabWidget                                                  │
├───────────────────────────────────────────────────────────┤
│ QStatusBar (MainWindow)                                     │
│  ✅ 0  ❌ 0  ⏱ 0s    │  (临时消息: 就绪/运行中/已暂停...)   │
└───────────────────────────────────────────────────────────┘
```

---

## 数据流

```
TestExecutionEngine
  │ engineStateChanged(EngineState)
  │ suiteFinished(name, pass, fail)
  └──────────────────────────→ MainWindow
                                  ├── syncControlStates()         → Ribbon 按钮 enabled
                                  ├── label_exec_stats_->setText() → permanent QLabel
                                  └── statusBar()->showMessage()   → 临时状态文字
```

stats 数据完全在 MainWindow 内部流转，不经过编辑器。

---

## 具体改动

### 文件清单

| 操作 | 文件 |
|------|------|
| **删除** | `src/test_program/ExecutionControlBar.h` |
| **删除** | `src/test_program/ExecutionControlBar.cpp` |
| **删除** | `src/app/resources/styles/execution_control_bar.qss` |
| **修改** | `src/app/resources/resource.qrc` — 移除 qss 引用 |
| **修改** | `src/test_program/TestProgramEditorWidget.h` — 移除 ExecutionControlBar 相关 |
| **修改** | `src/test_program/TestProgramEditorWidget.cpp` — 移除底部 bar |
| **修改** | `src/test_program/CMakeLists.txt` — 移除文件条目 |
| **修改** | `src/app/MainWindow.h` — 移除 `ExecutionControlBar*`，新增 `QLabel* label_exec_stats_` |
| **修改** | `src/app/MainWindow.cpp` — 引擎状态驱动 StatusBar |

---

### 详细改动

#### 1. TestProgramEditorWidget.h

**删除：**
```cpp
#include "ExecutionControlBar.h"
ExecutionControlBar* execution_control_bar_ = nullptr;
ExecutionControlBar* executionControlBar() const { return execution_control_bar_; }
```

#### 2. TestProgramEditorWidget.cpp

**删除底部 bar（原 L268-270）：**
```cpp
// 删掉这两行：
execution_control_bar_ = new ExecutionControlBar(content);
main_layout->addWidget(execution_control_bar_);
```

其他部分不变。工具栏、tab 等原样保留。

#### 3. MainWindow.h

**删除：**
```cpp
class ExecutionControlBar;                     // 前向声明
ExecutionControlBar* current_control_bar_ = nullptr;  // 成员变量
```

**新增：**
```cpp
QLabel* label_exec_stats_ = nullptr;           // StatusBar 统计标签
```

#### 4. MainWindow.cpp

**删除 `ExecutionControlBar` 引用（原 L2077）：**
```cpp
// 删掉：
current_control_bar_ = progEditor->executionControlBar();
```
`MainWindow` 不再需要通过编辑器获取控制栏——Ribbon 的 `act_run_` 等直接触发 `onRunClicked`，引擎状态的 `syncControlStates()` 已在 Ribbon 连接中处理按钮启用状态。

**新增 StatusBar permanent QLabel（在 `createStatusBar()` 或 `initUi()` 中）：**
```cpp
label_exec_stats_ = new QLabel(QStringLiteral("✅ 0  ❌ 0  ⏱ 0s"));
statusBar()->addPermanentWidget(label_exec_stats_);
```

**引擎状态驱动（在 `createEngine()` 的现有连接中补充）：**
```cpp
connect(engine_, &TestExecutionEngine::engineStateChanged,
        this, [this](EngineState state) {
  // syncControlStates() 已处理 Ribbon 按钮状态
  switch (state) {
    case EngineState::Idle:
      statusBar()->showMessage(QStringLiteral("就绪"));
      break;
    case EngineState::Running:
      statusBar()->showMessage(QStringLiteral("运行中..."));
      break;
    case EngineState::Paused:
      statusBar()->showMessage(QStringLiteral("已暂停"));
      break;
    case EngineState::Finished:
      statusBar()->showMessage(
          QStringLiteral("执行完成 (✅%1 ❌%2)")
              .arg(pass_count_).arg(fail_count_));
      break;
    case EngineState::Error:
      statusBar()->showMessage(QStringLiteral("执行出错"));
      break;
  }
});

// suiteFinished → 更新统计标签
connect(engine_, &TestExecutionEngine::suiteFinished,
        this, [this](const QString& /*name*/, int pass, int fail) {
  pass_count_ += pass;
  fail_count_ += fail;
  if (label_exec_stats_) {
    label_exec_stats_->setText(
        QStringLiteral("✅ %1  ❌ %2  ⏱ %3s")
            .arg(pass_count_).arg(fail_count_)
            .arg(elapsed_ms_ / 1000));
  }
});
```

> 注意：`pass_count_`、`fail_count_`、`elapsed_ms_` 的累加逻辑可能需要根据引擎实际发射的信号补充（如 `stepFinished` 的状态判断）。

#### 5. CMakeLists.txt

```cmake
# src/test_program/CMakeLists.txt
# 删除：
#    ExecutionControlBar.cpp
#    ExecutionControlBar.h
```

#### 6. QSS + resource.qrc

- 删除 `src/app/resources/styles/execution_control_bar.qss`
- 从 `src/app/resources/resource.qrc` 中移除对应的 `<file>` 条目
- 无需迁移样式——QAction 不使用 QPushButton 样式，Ribbon action 有自己的样式体系

---

## 影响范围验证

| 检查项 | 状态 |
|--------|------|
| `ExecutionControlBar` 唯一创建点 → `TestProgramEditorWidget.cpp:269` | ✅ 删除 |
| `ExecutionControlBar` 唯一外部引用 → `MainWindow.cpp:2077` | ✅ 删除，Ribbon 直接驱动 |
| `ExecutionControlBar::setState()` 调用 → `MainWindow.cpp:1930-1947` | ✅ 改为 `engineStateChanged` |
| `ExecutionControlBar::updateStats()` 调用 | 从未被调用，无影响 |
| QSS 文件 | ✅ 删除 |
| `resource.qrc` 引用 | ✅ 同步移除 |
| CMake 条目 | ✅ 移除 |
| Ribbon `act_run_/act_pause_/act_stop_` | 不受影响 |
| `syncControlStates()` | 不受影响 |
| `MainWindow::onRunClicked()` 等 | 不受影响（不依赖 `current_control_bar_`） |
| 独立工具 `src/tools/test-program-editor` | 不受影响，从未引用 ExecutionControlBar |

---

## 执行顺序

```
1. MainWindow.h       — 删除前向声明 + 新增 label_exec_stats_
2. MainWindow.cpp     — 补充引擎状态连接 + permanent QLabel
3. TestProgramEditorWidget.h  — 删除 ExecutionControlBar 成员和 include
4. TestProgramEditorWidget.cpp — 删除底部 bar 两行
5. 删除 ExecutionControlBar.h/.cpp
6. 修改 CMakeLists.txt         — 移除文件条目
7. 删除 QSS + 更新 resource.qrc
8. 构建验证                    — 全量编译 + CLI 测试
```

---

## 回滚方案

```bash
git stash
# 恢复 execution_control_bar.qss（git stash 会保留未跟踪文件需手动）
git checkout -- src/app/resources/resource.qrc
# 重新构建
```
