# visualizer 与 runconfig 合并为一个静态库方案

## 问题陈述

当前仓库存在两个相关的静态库：

- **etest_visualizer**（`src/visualizer/`）：VisualizationArea 画布 + 9 对源文件（VisualizationArea +
  `visualizers/` 下 8 个：SignalVisualizer 基类 + 5 个具体控件 + VisualizerFactory + VisualizerProxy）
- **etest_runconfig**（`src/runconfig/`）：RunConfig 数据模型 + RunConfigEditor + 3 个私有控件

两者被以下消费者使用：

| 消费者 | 使用的组件 |
| --- | --- |
| RunConfigEditor（编辑态画布） | VisualizationArea + visualizers |
| ExecutionDashboard（app 运行态） | VisualizationArea |
| ExecutionPanelController（app 运行态） | VisualizationArea + SignalVisualizer 等 |
| **test-executor（独立运行程序，未来）** | RunConfig 模型 + VisualizationArea（纯只读展示） |

分析：VisualizationArea 及其 visualizers 是 RunConfigEditor 的主干（编辑器主体就是画布 + dock +
数据声明），同时被 app 运行态共用。二者在语义上同属「运行配置/监控」领域，是同一类功能的 UI。

## 架构回顾

```
etest (主程序)
├── etest_runconfig (合并后)
│   ├── RunConfig                (.erun 数据模型)
│   ├── RunConfigEditor          (编辑器主体)
│   ├── VisualizationArea        (画布，原 etest_visualizer)
│   ├── visualizers/             (SignalVisualizer + 5 子类，原 etest_visualizer)
│   ├── ProgramChecklistWidget
│   ├── VisualizerPaletteWidget
│   └── MonitorPropertyWidget
└── test-executor (独立运行程序，未来)
    └── 链接 etest_runconfig，只消费 RunConfig 模型 + VisualizationArea（纯只读）
```

依赖方向（合并后 etest_runconfig）：

```
etest_runconfig
    ├── etest_api        (IEditor 接口)
    ├── etest_core       (ProjectManager / Logger)
    ├── etest_core_ui    (AppIconProvider / ThemeManager)
    ├── etest_engine     (MonitorSample，visualizers 采样数据源)
    ├── etest_ui         (DockTitleBar)
    └── Qt5::Widgets / qcustomplot
```

无循环依赖，不碰 topology/protocol/program，不违反附属工具约束。

## 方案选项

### 选项 A：合并 visualizer 与 runconfig 为一个库（推荐）

将 `src/visualizer/` 的 9 个源文件对（VisualizationArea + `visualizers/` 下 8 个）迁入 `src/runconfig/`，
统一目标 `etest_runconfig`，命名空间统一 `etest::runconfig`。app 侧引用随之改名。

优点：
- 语义内聚：「运行配置领域」库 = 数据模型 + 编辑器 + 监控可视化组件，与拓扑/协议/测试程序
  编辑器「一类功能 UI 聚合一个库」的设计理念一致
- 消费者完整：主程序编辑态/运行态 + 未来 test-executor 从同一库各取所需，无需第二产品重复链接
- 静态库符号裁剪：test-executor 不引用 RunConfigEditor 符号，编辑器代码不进其产物，无包袱

缺点：命名空间改动波及 app 4 个文件（ExecutionDashboard/ExecutionPanelController）及 runconfig 内
3 个消费文件（RunConfigEditor.h/.cpp、VisualizerPaletteWidget.cpp）；增量编译粒度变大。

### 选项 B：保持两个库分离（现状）

理由曾被「test-executor 独立复用」支撑，但查证 test-executor 目前**不链接 etest_visualizer**，
且未来定位是纯只读展示（消费 RunConfig 模型 + VisualizationArea），并非可视化组件库的独立消费者。
「分层复用」在只有主程序一个产品消费时无实际收益。不推荐。

### 选项 C：visualizer 并入 runconfig，但保留 etest::visualizer 命名空间

合并目标但保留两个命名空间。

缺点：一个库两个语义命名空间，违背「一个库一个语义」；app 仍写 `etest::visualizer::` 前缀，
改名收益为零。不推荐。

## 决策记录

| 决策 | 结论 | 理由 |
| --- | --- | --- |
| 合并 | 是 | 同属运行配置领域，一类功能 UI 聚合一个库 |
| 库名 | `etest_runconfig` | 与 etest_topology/etest_protocol/etest_program 一致 |
| 命名空间 | 统一 `etest::runconfig` | 一个库一个语义，避免残留双命名空间 |
| 目录 | `src/runconfig/`，`visualizers/` 子目录保留 | VisualizationArea 平铺 + visualizers/ 与现状一致 |
| app 运行态 | 引用 `etest::runconfig::VisualizationArea` 等 | 本就是主程序一部分，无跨产品问题 |
| test-executor | 本次不改，未来链接 etest_runconfig 纯只读展示 | 当前无可视化界面，链接保持不变；未来实现时禁用 VisualizationArea 编辑交互 |

## 实施计划

### T1 迁入源文件

将 `src/visualizer/` 全部 9 个源文件对（VisualizationArea + visualizers/ 下 8 个）git mv 到
`src/runconfig/`（visualizers/ 子目录结构保留）。删除 `src/visualizer/` 目录及其 CMakeLists.txt。

迁移清单（git mv，保留历史）：
- `VisualizationArea.h/.cpp` → `src/runconfig/VisualizationArea.h/.cpp`
- `visualizers/SignalVisualizer.h/.cpp` → `src/runconfig/visualizers/SignalVisualizer.h/.cpp`
- `visualizers/ValueLabelWidget.h/.cpp` → `src/runconfig/visualizers/ValueLabelWidget.h/.cpp`
- `visualizers/WaveformWidget.h/.cpp` → `src/runconfig/visualizers/WaveformWidget.h/.cpp`
- `visualizers/DigitalMeterWidget.h/.cpp` → `src/runconfig/visualizers/DigitalMeterWidget.h/.cpp`
- `visualizers/GaugeVisualizer.h/.cpp` → `src/runconfig/visualizers/GaugeVisualizer.h/.cpp`
- `visualizers/LedIndicator.h/.cpp` → `src/runconfig/visualizers/LedIndicator.h/.cpp`
- `visualizers/VisualizerFactory.h/.cpp` → `src/runconfig/visualizers/VisualizerFactory.h/.cpp`
- `visualizers/VisualizerProxy.h/.cpp` → `src/runconfig/visualizers/VisualizerProxy.h/.cpp`

### T2 改命名空间

src/runconfig/ 内全部迁入文件的 `namespace etest::visualizer` → `namespace etest::runconfig`。
涉及：VisualizationArea、SignalVisualizer + 5 子类、VisualizerFactory、VisualizerProxy。

注意保留 `namespace etest::engine`（MonitorSample 类型）等非 visualizer 命名空间不变。

### T3 更新 CMake

- `src/runconfig/CMakeLists.txt`：源列表追加 9 对文件；**删除 `etest_visualizer` 链接**（该 target 合并后不复存在），
  **以 PUBLIC 追加 `etest_engine`、`qcustomplot`**（与现 etest_visualizer 保持一致，保证 test-executor 未来
  只链 etest_runconfig 消费 VisualizationArea 也能拿到传递依赖）；同步更新文件头注释中「依赖 etest_visualizer」的表述
- `src/CMakeLists.txt`：删除 `add_subdirectory(visualizer)`
- `src/app/CMakeLists.txt`：链接库列表删除 `etest_visualizer`（保留 etest_runconfig）

### T4 更新 app 侧引用

4 个文件把 `etest::visualizer` → `etest::runconfig`：
- `src/app/ExecutionDashboard.h`：前置声明块 + `visualizationArea()` + `vis_area_` 成员
- `src/app/ExecutionDashboard.cpp`：`using etest::visualizer::VisualizationArea;` + include
- `src/app/ExecutionPanelController.h`：前置声明块 + `subscribeVisualizer` 参数
- `src/app/ExecutionPanelController.cpp`：4 处 `using etest::visualizer::*`（SignalVisualizer/VisualizationArea/
  WaveformWidget/createVisualizerFor）+ include，并同步更新 L44 注释「可视化区下沉共享层（etest_visualizer）类型」

include 路径 `visualizer/VisualizationArea.h` → `runconfig/VisualizationArea.h`；
`visualizer/visualizers/SignalVisualizer.h` → `runconfig/visualizers/SignalVisualizer.h`（在 runconfig
模块内可省前缀，app 侧需写全路径）。

### T5 更新 runconfig 内引用

`src/runconfig/RunConfigEditor.cpp/.h`、`VisualizerPaletteWidget.cpp` 中 `etest::visualizer` →
`etest::runconfig`，include `visualizer/...` → 同目录/`visualizers/...`；并同步更新
`RunConfigEditor.cpp` L48 注释「可视化区下沉共享层（etest_visualizer）类型」（与 T4 处理的
ExecutionPanelController.cpp L44 同款，指向已不存在的 target 名）。

### T6 编译验证

```bash
scripts/build_ninja.bat -t debug -m ETestStudio
```

全量编译通过后，运行 `ctest -R test_runconfig_run_config` 确认单测通过。
注：该单测直接编译 `src/runconfig/RunConfig.cpp`，仅验证 RunConfig 模型，**不覆盖合并后的
visualizer 代码**——合并后库的正确性主要靠「全量编译通过」保证。

## 验证

- 编译通过（app 全量 + 单测）
- 编辑器功能不回归：拖放建卡 / 属性编辑 / 保存 / 运行态全建卡
- QSS 不受影响：各主题 QSS 对可视化区均用 objectName 选择器 `#VisualizationArea`，
  无 `etest--visualizer--` 命名空间限定选择器，重命名命名空间不破坏样式

## 风险与回退

- 命名空间改动是纯机械替换，风险低；git 恢复即可回退
- `etest_engine`/`qcustomplot` 依赖通过 etest_visualizer 的 PUBLIC 链接已传递到 runconfig，
  T3 明确列出避免隐式依赖
- clangd 无 MSVC STL 误报 `type_traits file not found` 属已知噪音，以 MSVC 编译为准
