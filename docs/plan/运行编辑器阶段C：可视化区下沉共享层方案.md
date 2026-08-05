# 运行编辑器阶段 C：可视化区下沉共享层方案

> 版本：v3（命名修订版）。模块名由 visualizer 改为 **visualizer**（`src/visualizer` + `etest::visualizer` + `etest_visualizer`）：可视化区是 UI 展示层，避免与 `etest_engine`（执行引擎）混淆。v2 已修 🔴1 依赖链补 `etest_core_ui`、🟡1-4。

## 一、问题陈述

阶段 A/B 已完成：`.erun` 成为运行配置唯一真源，运行编辑器（编辑态）与运行态（ExecutionDashboard）都消费可视化区。但**可视化区（`VisualizationArea` + `VisualizerProxy` + `visualizers/`）目前寄居在 `src/app`**（etest 主程序 target 内），与 app 层耦合：

- 阶段 D 的独立运行程序（`src/tools/test-executor`）要复用可视化区展示 `.erun`，却无法链接——可视化区在 app target 里，独立程序不能链 app。
- 主程序运行态与独立运行程序是"编辑器 + 运行器"架构的两个消费方，共享核心是可视化区，它应独立成共享模块。

**目标**：可视化区从 `src/app` 下沉为独立共享模块 `src/visualizer`（target `etest_visualizer`），主程序运行态 + 独立运行程序共同消费；命名空间从 `etest::app` 改为 `etest::visualizer`（模块归属清晰）。阶段 C 只做下沉 + 主程序接入（行为不变），test-executor 接入留阶段 D。

## 二、现状梳理（依赖边界分析）

### 2.1 下沉文件（当前在 `src/app`，9 对）

| 文件 | 职责 | 外部依赖 |
|---|---|---|
| `VisualizationArea.h/.cpp` | QGraphicsView 画布，卡片布局/交互/排列分布 | Qt（QGraphicsView）+ `SignalVisualizer` + `VisualizerProxy` |
| `visualizers/VisualizerProxy.h/.cpp` | `QGraphicsProxyWidget` 卡片（移动/8 向 resize 手柄） | 纯 Qt |
| `visualizers/SignalVisualizer.h/.cpp` | 可视化组件抽象基类 | `etest::engine::MonitorSample`（engine） |
| `visualizers/WaveformWidget.h/.cpp` | 波形图 | **QCustomPlot** + `logger/Logger.h`（core）+ `core_ui/ThemeManager.h`（core_ui）+ `engine/MonitorManager.h` |
| `visualizers/ValueLabelWidget.h/.cpp` | 数值/帧显示 | `engine/MonitorManager.h`（头文件，仅取 MonitorSample） |
| `visualizers/DigitalMeterWidget.h/.cpp` | 数字仪表 | `engine/MonitorManager.h` |
| `visualizers/GaugeVisualizer.h/.cpp` | 指针表盘 | `ThemeManager.h`（core_ui）+ `engine/MonitorManager.h` |
| `visualizers/LedIndicator.h/.cpp` | LED 指示灯 | `engine/MonitorManager.h` |
| `visualizers/VisualizerFactory.h/.cpp` | `createVisualizerFor` 按 displayMode/signalType 创建组件 | 上述 visualizers |

### 2.2 依赖干净性确认（关键结论）

对 `src/app/visualizers/` 全目录 grep `AppIconProvider`、`logger`、`ThemeManager`、`core_ui/`、`engine/`、`VisualizationArea`、`app/`、`editors/`、`widgets/`、`dialogs/`：

- **无 app 特有依赖**：无 `AppIconProvider`、无 `app/`/`editors/`/`widgets/`/`dialogs/` 头、无 `:/` 资源引用。
- **非 Qt 依赖均为共享模块**：`logger/Logger.h`（etest_core，仅 WaveformWidget）、`core_ui/ThemeManager.h`（etest_core_ui，WaveformWidget/GaugeVisualizer）、`engine/MonitorManager.h`（etest_engine，5 个具体组件，仅取 `MonitorSample` 类型）。

即**可视化区可以整体搬走，无需解耦 app 特有依赖**；下沉 target 需链接 `etest_core_ui`（见 4.3）。

### 2.3 消费方（app 内引用，需改 include + 命名空间）

| 消费方文件 | 引用的下沉符号 |
|---|---|
| `src/app/ExecutionDashboard.h/.cpp` | `VisualizationArea`（成员 `vis_area_` + `visualizationArea()` 返回类型） |
| `src/app/ExecutionPanelController.cpp` | `VisualizationArea`、`SignalVisualizer`、`VisualizerFactory::createVisualizerFor`、`WaveformWidget` |
| `src/app/ExecutionPanelController.h` | `SignalVisualizer`（前向声明 + 方法参数/成员） |
| `src/app/editors/RunConfigEditor.cpp` | `VisualizationArea`、`VisualizerFactory::createVisualizerFor` |
| `src/app/editors/RunConfigEditor.h` | `VisualizationArea`（成员 `vis_area_`） |
| `src/app/dialogs/MonitorConfigDialog.cpp` | 5 个具体 visualizer（预览瓦片）+ `SignalVisualizer` |
| `src/app/dialogs/MonitorConfigDialog.h` | `SignalVisualizer`（前向声明 + `createPreviewVisualizer` 返回类型） |

### 2.4 test-executor 现状

`src/tools/test-executor` 为 WIN32 骨架（仅 `main.cpp`），链接 `etest_engine/etest_core/etest_core_ui/etest_ui`。**阶段 C 不动它**，阶段 D 演进时链接 `etest_visualizer` 即可。

## 三、决策记录（已确认）

| # | 决策 | 说明 |
|---|---|---|
| C-1 | 新模块 `src/visualizer`，target `etest_visualizer` | 命名体现 UI 展示层（visualizer 组件），避免与 `etest_engine` 执行引擎混淆 |
| C-2 | 命名空间从 `etest::app` 改 `etest::visualizer` | 模块归属清晰；机械性改所有消费方引用 |
| C-3 | 阶段 C 范围 = 下沉 + 主程序接入（行为不变），test-executor 接入留阶段 D | 自含验收，风险可控 |
| C-4 | `RunConfigEditor` 编辑器壳留 `src/app/editors/` | 独立运行程序不需要编辑能力（"运行=定稿"） |

## 四、方案设计

### 4.1 新模块结构

```
src/visualizer/
├── CMakeLists.txt          # add_library(etest_visualizer STATIC ...)
├── VisualizationArea.h/.cpp
└── visualizers/
    ├── SignalVisualizer.h/.cpp
    ├── VisualizerProxy.h/.cpp
    ├── WaveformWidget.h/.cpp
    ├── ValueLabelWidget.h/.cpp
    ├── DigitalMeterWidget.h/.cpp
    ├── GaugeVisualizer.h/.cpp
    ├── LedIndicator.h/.cpp
    └── VisualizerFactory.h/.cpp
```

文件从 `src/app/` 与 `src/app/visualizers/` **整体移动**（git mv，保留历史），仅改命名空间，不改逻辑。

### 4.2 命名空间迁移

**下沉文件**（`src/visualizer` 内）：
- 全部 `namespace etest::app { }` → `namespace etest::visualizer { }`；5 个 `#ifndef ETEST_APP_VISUALIZERS_*_H_` 头保护符转 `#pragma once`（见开放问题 1）。
- 内部互引用（`WaveformWidget.h` include `SignalVisualizer.h`、`LedIndicator.h` include `visualizers/SignalVisualizer.h`）同步改路径前缀 `visualizer/visualizers/` 或 `visualizer/` 与命名空间。
- `VisualizationArea.h` 仅含 Qt 头（`SignalVisualizer.h`/`VisualizerProxy.h` 在 `VisualizationArea.cpp`）。

**消费方**（留在 `etest::app`，四、2.3 清单）——两步，不能只改前缀（全仓 grep `etest::app::` 前缀引用**零命中**，无现成前缀可机械替换）：
1. **头文件前向声明**：`ExecutionDashboard.h`/`ExecutionPanelController.h`/`RunConfigEditor.h`/`MonitorConfigDialog.h` 内 `namespace etest::app { }` 里的 `class SignalVisualizer;`/`class VisualizationArea;` 移到新的 `namespace etest::visualizer { ... }` 块（这些类型下沉后不再存在于 `etest::app`）。
2. **未限定使用**：消费方 .h/.cpp 内所有未限定的 `SignalVisualizer`/`VisualizationArea`/`VisualizerProxy`/`VisualizerFactory`/具体 visualizer 类型改为 `etest::visualizer::` 全限定，或注入 `using etest::visualizer::SignalVisualizer;` 等。
3. **include 路径**：`"VisualizationArea.h"` → `"visualizer/VisualizationArea.h"`；`"visualizers/SignalVisualizer.h"` → `"visualizer/visualizers/SignalVisualizer.h"`。

### 4.3 依赖链

`etest_visualizer` 链接：

```
etest_visualizer
├── Qt5::Widgets（+ Core/Gui，AUTOMOC）
├── qcustomplot（3rdparty，WaveformWidget 用）
├── etest_engine（MonitorSample：SignalVisualizer 基类 + 5 个具体组件头）
├── etest_core（logger：WaveformWidget）
└── etest_core_ui（ThemeManager：WaveformWidget/GaugeVisualizer）
```

app（etest）链接 `etest_visualizer`，替代当前对 visualizers/VisualizationArea 源文件的直接注册。

### 4.4 消费方更新（app）

四、2.3 的 4 个 .cpp + 3 个 .h 改 include 路径与命名空间前缀。改动纯机械，无逻辑变化。

### 4.5 CMake 接线

- 新增 `src/visualizer/CMakeLists.txt`：`add_library(etest_visualizer STATIC ...)`，注册 9 对文件，`target_include_directories` 含 `${CMAKE_SOURCE_DIR}/src`，AUTOMOC ON，链接四、4.3 依赖。
- `src/CMakeLists.txt`：`add_subdirectory(visualizer)`（src 模块聚合处，engine 后）。
- `src/app/CMakeLists.txt`：删除 visualizers/ 8 对 + `VisualizationArea.h/.cpp` 源注册（:119-136），`target_link_libraries` 加 `etest_visualizer`。

### 4.6 不动的部分（下沉不改变行为）

- **QSS**：`VisualizationArea` 的 `setObjectName("VisualizationArea")` 不变，`#VisualizationArea` 选择器在 app 资源 QSS 中继续匹配（QSS 按 objectName 匹配，与模块归属无关）。
- **visualizers 内部 objectName/配色**：不变。
- **`createVisualizerFor` 映射规则**：不变（waveform/led/meter/frame + auto 推断）。
- **运行态/编辑态交互语义**：不变（手动+只读 / 手动+可编辑 / 自动网格）。

## 五、影响范围

| 文件 | 改动 |
|---|---|
| `src/visualizer/CMakeLists.txt` | **新增**：`etest_visualizer` target |
| `src/visualizer/VisualizationArea.h/.cpp` | 从 `src/app` 移动 + `etest::app` → `etest::visualizer` |
| `src/visualizer/visualizers/*`（8 对） | 从 `src/app/visualizers` 移动 + 命名空间改 |
| `src/app/ExecutionDashboard.h/.cpp` | include 改 `visualizer/VisualizationArea.h` + 命名空间 |
| `src/app/ExecutionPanelController.h/.cpp` | include 改 + `etest::visualizer::` 前缀 |
| `src/app/editors/RunConfigEditor.h/.cpp` | include 改 + `etest::visualizer::` 前缀 |
| `src/app/dialogs/MonitorConfigDialog.h/.cpp` | include 改 + `etest::visualizer::` 前缀 |
| 根 `CMakeLists.txt` | `add_subdirectory(src/visualizer)` |
| `src/app/CMakeLists.txt` | 删 visualizers/VisualizationArea 源注册；链接 `etest_visualizer` |
| `src/app/visualizers/`（原目录） | **删除**（文件已移动） |

## 六、开放问题

1. **头保护符**：5 个 `#ifndef ETEST_APP_VISUALIZERS_*_H_`（SignalVisualizer/VisualizerFactory/DigitalMeterWidget/ValueLabelWidget/WaveformWidget）已统一转 `#pragma once`（符合 CLAUDE.md 规则）；其余 4 个文件本就 `#pragma once`。
2. **`etest::app` 其他引用**：下沉文件内部是否有遗漏的 `etest::app::` 限定（如 `etest::app::SignalVisualizer` 全限定引用）？——实施期全仓 grep `etest::app` 在 visualizer 目录确认清零。
3. **MonitorConfigDialog 预览瓦片**：对话框用具体 visualizer 实例做预览（`createPreviewVisualizer`），下沉后走 `etest::visualizer` 类型，QSS 预览样式是否受影响？——objectName 不变，预期不受影响，手动验证确认。
4. **app 对其他模块的隐式依赖**：app 现直接 include `visualizers/`，下沉后 app 是否还隐式依赖 `qcustomplot`/`etest_engine`？——app 本身已链接 qcustomplot/etest_engine，无新增。

## 七、实施计划

### C1 建模块 + 移动文件

- **文件**：新增 `src/visualizer/CMakeLists.txt`；`git mv src/app/VisualizationArea.h/.cpp src/visualizer/`；`git mv src/app/visualizers/*.h/.cpp src/visualizer/visualizers/`
- 建 `src/visualizer/CMakeLists.txt`：注册 9 对文件 + 依赖（四、4.3）
- **验证**：CMake 配置通过

### C2 命名空间迁移

- **文件**：`src/visualizer/` 下 9 对文件
- 全部 `namespace etest::app` → `etest::visualizer`；5 个 `#ifndef ETEST_APP_VISUALIZERS_*_H_` 转 `#pragma once`（SignalVisualizer/VisualizerFactory/DigitalMeterWidget/ValueLabelWidget/WaveformWidget）；内部互引用 include 靠 `src/visualizer` include 路径兜底（保持相对路径）
- 统一 `ThemeManager.h` include 为 `core_ui/ThemeManager.h`（`GaugeVisualizer.cpp` 现用未限定 `"ThemeManager.h"`，依赖 etest_core_ui PUBLIC include 传播）
- 全仓 grep `etest::app` 在 `src/visualizer` 清零（开放问题 2）
- **验证**：单测编译 visualizer 通过

### C3 消费方更新（app）

- **文件**：`ExecutionDashboard.h/.cpp`、`ExecutionPanelController.h/.cpp`、`RunConfigEditor.h/.cpp`、`MonitorConfigDialog.h/.cpp`
- include 改 `visualizer/...`；`etest::app::` 前缀改 `etest::visualizer::`
- **验证**：`scripts/build_ninja.bat -t debug -m ETestStudio` 编译通过

### C4 CMake 接线 + 清理

- **文件**：根 `CMakeLists.txt`（add_subdirectory）、`src/app/CMakeLists.txt`（删源注册、加链接）
- 删空目录 `src/app/visualizers/`
- **验证**：全量编译（`-t debug`）所有 target 通过

### C5 行为验证

- 主程序编译运行，`demo_mock` 手动验证：运行编辑器可视化区手动布局、运行态按 `.erun` 还原布局（手动+只读）、监听器卡片展示，与阶段 B 行为一致
- **验证**：无回归（QSS objectName 选择器、布局/交互语义不变）

## 八、阶段规划更新

阶段 C 完成后，`etest_visualizer` 可被独立运行程序消费；阶段 D（test-executor 读 `.erun` 直接跑）前置依赖即满足。
