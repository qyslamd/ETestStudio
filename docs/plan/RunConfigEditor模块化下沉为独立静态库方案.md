# RunConfigEditor 模块化下沉为独立静态库方案

## 问题陈述

运行编辑器（RunConfigEditor）及其相关代码目前分散在 `src/app` 内多个位置：

- `src/app/editors/RunConfig.h/.cpp` — `.erun` 数据模型
- `src/app/editors/RunConfigEditor.h/.cpp` — 编辑器主体
- `src/app/widgets/ProgramChecklistWidget.h/.cpp` — 测试程序多选面板
- `src/app/widgets/VisualizerPaletteWidget.h/.cpp` — 调色板
- `src/app/widgets/MonitorPropertyWidget.h/.cpp` — 属性面板

这些文件虽然功能上属于同一「运行配置编辑」领域，却混在 app 的通用 editors/widgets 目录里，与
TextEditorWidget、ImageViewerWidget、ProblemsPanel 等无关组件同级。对比拓扑编辑器（`src/topology/`
→ `etest_topology`）、协议编辑器（`src/protocol/` → `etest_protocol`）、测试程序编辑器
（`src/test_program/` → `etest_program`）均各自独立为静态库，运行编辑器是唯一仍嵌在 app 里的编辑器。

## 架构回顾

```
etest (主程序)
├── etest_runconfig (新建，本次目标)
│   ├── RunConfig            (数据模型，纯 Qt + etest_core)
│   ├── RunConfigEditor      (编辑器主体，依赖 etest_visualizer)
│   ├── ProgramChecklistWidget
│   ├── VisualizerPaletteWidget
│   └── MonitorPropertyWidget
├── etest_visualizer     (VisualizationArea / VisualizerProxy / visualizers)
├── etest_topology / etest_protocol / etest_program
├── etest_ui / etest_core / etest_core_ui / etest_api
└── Qt5 / 第三方库
```

依赖方向：

```
etest_runconfig
    ├── etest_api        (IEditor 接口)
    ├── etest_core       (ProjectManager / Logger)
    ├── etest_core_ui    (AppIconProvider)
    ├── etest_visualizer (VisualizationArea / VisualizerProxy / SignalVisualizer / VisualizerFactory)
    ├── etest_ui         (DockTitleBar)
    └── Qt5::Widgets
```

无循环依赖。运行态消费链（ExecutionPanelController）留在 app，通过 `etest::runconfig::RunConfig`
模型消费 `.erun`，不引入额外依赖。

## 方案选项

### 选项 A：RunConfigEditor + 模型 + 3 私有控件整体下沉（推荐）

- 目录 `src/runconfig/`，CMake 目标 `etest_runconfig`（STATIC），命名空间 `etest::runconfig`
- 迁入：`RunConfig.h/.cpp`、`RunConfigEditor.h/.cpp`、`ProgramChecklistWidget.h/.cpp`、
  `VisualizerPaletteWidget.h/.cpp`、`MonitorPropertyWidget.h/.cpp`
- 运行态消费链留在 `ExecutionPanelController`，只改类型引用（`etest::app::RunConfig` → `etest::runconfig::RunConfig`）
- 单测 `tests/app/run_config_test.cpp` 迁至 `tests/runconfig/`

优点：
- 与拓扑/协议/测试程序编辑器边界一致（编辑器及其私有控件自包含）
- 依赖全部是公共模块，无 app 私有耦合，下沉零障碍
- 为未来独立运行程序（test-executor 类产品）直接复用编辑器铺路

缺点：改动涉及 5 对文件 + app 侧引用 + 单测，是一次中等规模重构。

### 选项 B：仅迁移 RunConfig 模型，编辑器留在 app

将 `RunConfig.h/.cpp` 下沉为 `etest_runconfig` 库，编辑器及相关控件留在 app。

优点：改动最小。

缺点：编辑器与模型分居两处，模块边界不完整；模型库与编辑器库将来要合并，是半途状态。不推荐。

### 选项 C：运行态消费链一并下沉

把 `createMonitorCard`/`loadProjectMonitors`/`subscribeVisualizer` 等也放进 `etest_runconfig`。

缺点：运行态消费链依赖 `etest::engine`（TestExecutionEngine/MonitorManager）及 app 内
`ExecutionDashboard`/`ExecutionOutputPanel`/`AppStatusBarController` 等组件，会把 app 层依赖拖进模块，
形成 runconfig → app 的反向依赖，违背分层与依赖倒置。不推荐。

## 决策记录

| 决策 | 结论 | 理由 |
| --- | --- | --- |
| 目录与目标名 | `src/runconfig/` + `etest_runconfig` | 与 `etest_topology`/`etest_protocol`/`etest_program` 命名一致 |
| 命名空间 | `etest::runconfig` | CLAUDE.md 要求 etest 系列模块统一 `etest::xxx` |
| 运行态消费链 | 留在 app | 依赖 engine + dashboard，下沉违背依赖倒置 |
| 私有控件归属 | 随编辑器下沉 | 仅 RunConfigEditor 使用，无其他消费者 |

## 实施计划

### T1 新建 `src/runconfig/CMakeLists.txt` 与目录骨架

新建 `src/runconfig/` 目录，创建 `CMakeLists.txt`（STATIC 库，AUTOMOC ON），
PUBLIC include 路径 `src/` 与 `src/runconfig/`，PUBLIC 链接：
`Qt5::Widgets Qt5::Core Qt5::Gui etest_api etest_core etest_core_ui etest_visualizer etest_ui`。

- 在 `src/CMakeLists.txt` 中 `add_subdirectory(runconfig)`（放 visualizer 之后、test_program 之前）

### T2 迁移 5 对源文件并改命名空间

将以下文件从 `src/app/` 迁移至 `src/runconfig/`：

- `editors/RunConfig.h/.cpp` → `runconfig/RunConfig.h/.cpp`
- `editors/RunConfigEditor.h/.cpp` → `runconfig/RunConfigEditor.h/.cpp`
- `widgets/ProgramChecklistWidget.h/.cpp` → `runconfig/ProgramChecklistWidget.h/.cpp`
- `widgets/VisualizerPaletteWidget.h/.cpp` → `runconfig/VisualizerPaletteWidget.h/.cpp`
- `widgets/MonitorPropertyWidget.h/.cpp` → `runconfig/MonitorPropertyWidget.h/.cpp`

每个文件：命名空间 `etest::app` → `etest::runconfig`（包括 `}  // namespace etest::app` 注释）。

include 调整（迁移后同目录引用必须去掉前缀）：
- `MonitorPropertyWidget.h` 的 include 由 `"editors/RunConfig.h"` 改为 `"RunConfig.h"`（同目录）
- `RunConfigEditor.cpp` 的 3 处 `"widgets/MonitorPropertyWidget.h"`、`"widgets/ProgramChecklistWidget.h"`、`"widgets/VisualizerPaletteWidget.h"` 改为同目录 `"Xxx.h"`
- `RunConfigEditor.h` 的 `namespace etest::visualizer { class VisualizationArea; }` 前置声明块**原样保留**（无 include 依赖，仅编译期引用）
- `RunConfigEditor.h` 继承改写为 `public etest::app::IEditor`（全限定）：`IEditor` 声明在 `etest::app`
  （api 层既有事实），模块下沉后 `etest::runconfig` 内裸 `IEditor` 解析不到，必须全限定，否则 C2504/override 全失效

### T3 调整 app 侧引用

- `src/app/CMakeLists.txt`：删除 5 对源文件条目，链接库列表加 `etest_runconfig`
- `src/app/EditorManager.cpp`：include `"editors/RunConfigEditor.h"` → `"runconfig/RunConfigEditor.h"`；
  **在 `namespace etest::app {` 块内首行**（首次使用前）加 `using etest::runconfig::RunConfigEditor;`
  ——using 必须置于命名空间块内，文件作用域会编译错（L135/140/145/853 共 4 处未限定名使用）
- `src/app/ExecutionPanelController.h`：include `"editors/RunConfig.h"` → `"runconfig/RunConfig.h"`；
  **在 `namespace etest::app {` 块内首行**加 `using etest::runconfig::RunConfig;`（或全限定 `etest::runconfig::RunConfig`）
- `src/app/ExecutionPanelController.cpp`：核对所有 `RunConfig` 引用在新 using 下无需改动

### T4 迁移单测

- 新建 `tests/runconfig/CMakeLists.txt`，把 `tests/app/run_config_test.cpp` 迁为
  `tests/runconfig/run_config_test.cpp`，目标名沿用 `test_app_run_config`（或改为 `test_runconfig_run_config`，需与 T5 一致）
- 源文件路径指向 `src/runconfig/RunConfig.cpp`；**需保留 `INCLUDES ${CMAKE_SOURCE_DIR}/src`
  （RunConfig.cpp 依赖 `logger/Logger.h`）并追加 `src/runconfig`**，`LIBS etest_core`，`QT_MODULES Core Test`
- `tests/app/CMakeLists.txt` 删除 run_config_test 条目；`tests/CMakeLists.txt` 加 `add_subdirectory(runconfig)`
- 测试内 `using namespace etest::app;` → `using namespace etest::runconfig;`
- 测试内 `#include "editors/RunConfig.h"` → `#include "RunConfig.h"`（INCLUDES 不再含 src/app）

### T5 编译验证

```bash
scripts/build_ninja.bat -t debug -m ETestStudio
```

全量编译通过后，追加运行单测目标确认 `test_app_run_config`（即 T4 定名）通过。

## 验证

- 编译通过（app 全量 + 单测）
- 编辑器功能不回归：拖放建卡 / 属性编辑 / 保存 / 运行态全建卡

## 风险与回退

- 命名空间改动是纯机械替换，风险低
- 若 `etest_runconfig` 与 `etest_visualizer` 之间出现隐式依赖（如 VisualizerFactory 使用未声明类型），
  编译期即可暴露；必要时将相关头文件依赖收敛到 `etest_visualizer` PUBLIC 传播
- 回退：git 恢复即可，不涉及数据格式变更
