# 指针表 GaugeVisualizer 组件方案

## 1. 问题陈述

执行仪表盘（ExecutionDashboard → VisualizationArea）现有 6 种信号可视化组件，数值类信号展示仅有 `DigitalMeterWidget`（纯文本大数字）。车速、转速、油压、温度等标量信号用模拟指针表更直观，贴合 HIL / 车辆仪表场景。

拟将 `D:\workspace\repos\qt-preview-demo\qpainter_draw_demoes\draw_gauge_car` 的 QPainter 模拟仪表盘（约 550 行）移植为新的 `SignalVisualizer` 组件。

现状痛点：

- 数字表缺少「量程 + 指针位置」的直观映射。
- 来源 demo 存在 `setValue` 失效 bug（setter 写 `value`、绘制读 `currentValue`）、构造时自启空转 QTimer、`animation`/`valueChanged` 死代码，不能直接搬。

## 2. 架构回顾

- `SignalVisualizer` 抽象基类：`onSampleCaptured(MonitorSample)` / `clearData()` / `displayedSignals()`，全部纯虚。
- `MonitorManager::subscribe` 按 monitorIndex 分发采样；`MonitorSample.engValue` 为工程值（double），即指针表的数据源。
- `VisualizerFactory::createVisualizerFor` 按 `displayMode`（waveform/led/meter/frame）分发，未知模式回退 auto 推断（按 signalType）。
- `displayMode` 存于拓扑 monitor 配置（`TopologyDocument.h:87`，默认 `"waveform"`），属性面板 `QComboBox` 选择（`PropertyPanelWidget.cpp:603-614`，`addItem(文案, 值)`），`TopologyJsonSerializer` 通用字符串序列化。
- `VisualizationArea`（QGraphicsView）网格布局、右键关闭、`visualizerClosed` 信号联动信号树。
- 主题：`ThemeManager` 单例提供语义色；QPainter 组件（如 `TabBarStyle`）直接从 `ThemeManager` 取色。
- 模块约束：topology 须可独立编译、不链接 app 模块。故 gauge 组件放 app 层（与其它 visualizer 一致），topology 仅存 `"gauge"` 字符串，无跨模块链接。

## 3. 方案选项与决策记录

### 决策 1：主题配色 —— 完全跟随主题语义色

选项：A 全跟随语义色 / B 固定深色盘面 + 主题点缀 / C 不主题化。

选定 A。理由：项目主题文化强，亮暗主题都需自然融合；与 `DigitalMeterWidget`（QSS 主题化）一致。颜色映射：

| 部件 | 来源 |
| ---- | ---- |
| 盘面（内圆） | `panelBackground` |
| 外圆 | `borderColor` |
| 饼弧内孔（coverCircle） | `panelBackground`（与盘面融合） |
| 三色饼弧（绿/黄/红） | 固定语义色（#22C55E / #F59E0B / #EF4444，与 QSS status 样式同源，跨主题一致） |
| 刻度 / 数值文字 | `textColor` |
| 指针 | `accentColor` |
| 中心圆 | `textColor` |
| 半透明高光遮罩（overlay） | 删除（亮/暗主题下易现光斑，见决策 4） |

### 决策 2：量程 —— 固定默认 0-100（代码常量）

选项：A 固定默认 / B 拓扑配置 min/max / C 自动跟踪。

选定 A。理由：`DigitalMeterWidget` 亦无量程来源，本期不扩拓扑配置面（YAGNI）；常量一处可调。`scaleMajor=10`、`scaleMinor=10`、`startAngle=45`、`endAngle=45`、`precision=0` 沿用默认。

### 决策 3：显示模式命名 —— `gauge` / 面板文案「指针表」

选项：A `gauge`/指针表 / B `gauge`/模拟仪表 / C `analog`/模拟仪表。

选定 A。理由：值与 `waveform/led/meter/frame` 同风格；文案 2 字与现有「波形/LED/仪表/帧数据」统一，且与已占用的「仪表」(meter) 明确区分。

### 决策 4：功能面 —— 保留 4 指针样式 + 2 饼图样式 API，删死代码

选项：A 全保留样式删死代码 / B 只留默认 / C 全保留并加配置 UI。

选定 A。理由：绘制分支已在，保留零成本；`animation`/`animationStep`/`reverse`/`valueChanged` 从未实现或发射，删；半透明高光遮罩 `drawOverlay` 一并删除（审查结论：亮/暗主题下易现光斑）；不做配置 UI（YAGNI），样式经公开 API 可编程控制即可（与 `StateLEDWidget` 等一致）。

### 决策 5：标题 —— 顶部显示信号名（QLabel + objectName 走 QSS）

选项：A 显示标题 / B 不显示。

选定 A。理由：与 `DigitalMeterWidget`/`ValueLabelWidget` 一致；单位本期不加（拓扑无单位元数据）。

### 决策 6：刷新 —— 每采样直接刷新

选项：A 每采样 repaint / B 定时器合并 / C 阈值死区。

选定 A。理由：与 `DigitalMeterWidget` 一致；绘制仅几十次 painter 操作，1 kHz 采样也可承受；实现最简单。

## 4. 实现方案

### 新增

`src/app/visualizers/GaugeVisualizer.h` / `.cpp`：

- `class GaugeVisualizer : public SignalVisualizer`，结构对齐 `WaveformWidget`：根控件（objectName `GaugeWidget`，QSS 背景 + `setAutoFillBackground(true)`）→ `QVBoxLayout` → 顶部标题 `QLabel`（objectName `GaugeTitle`）+ 独立子画布 `QWidget`（objectName `GaugeCanvas`，自绘盘面）
- 构造函数 `GaugeVisualizer(const QString& title, QWidget* parent = nullptr)`
- 移植 `GaugeCar` 绘制方法到 `GaugeCanvas::paintEvent`（`drawOuterCircle` / `drawInnerCircle` / `drawColorPie` / `drawCoverCircle` / `drawScale` / `drawScaleNum` / `drawPointer*` ×4 / `drawRoundCircle` / `drawCenterCircle` / `drawValue`，不含 `drawOverlay`），坐标归一化 `scale(side / 200.0)` 保留，子画布方形居中
- 颜色全部改从 `ThemeManager` 语义色取值（决策 1 映射），`paintEvent` 开头经 `applyThemeColors()` 刷新（无需连接 `themeChanged`，覆盖主题切换）
- 三色饼弧与当前值弧均以 `90 + start_angle_` 为起点对齐 value=0（修复 demo 色环落在底部与刻度错位的问题）
- 修复 demo 的 `setValue`/`value` 分离 bug：统一为 `current_value_`，`setValue(engValue)` 直接生效
- `onSampleCaptured`：`current_value_ = sample.engValue; update();`，记录 `monitor_index_`
- `clearData`：`current_value_ = min_value_; monitor_index_ = -1; update();`
- `displayedSignals`：`monitor_index_ >= 0` 时返回 `{monitor_index_}`
- 保留 `setPointerStyle` / `setPieStyle` 等公开 API（决策 4），默认 `PointerStyle_Indicator` + `PieStyle_Three`
- 删 `timer` / `animation` / `animationStep` / `reverse` / `valueChanged` / `drawOverlay`

### 修改

| 文件 | 改动 |
| ---- | ---- |
| `src/app/visualizers/VisualizerFactory.cpp` | `displayMode == "gauge"` → `new GaugeVisualizer(title, parent)` |
| `src/topology/PropertyPanelWidget.cpp:603-611` | combo 加 `addItem("指针表", "gauge")` |
| `src/topology/TopologyDocument.h:87` | 注释追加 gauge |
| `src/app/CMakeLists.txt:115-126` | 加 `visualizers/GaugeVisualizer.h` / `.cpp` |
| 13 个主题 `*.qss` | 分组背景规则加 `#GaugeWidget`；新增 `#GaugeTitle` 块（12px bold + 各主题 textColor） |

### 不动

- `VisualizationArea`（通用网格，无需改）
- `MonitorManager`（`engValue` 已有）
- `SignalVisualizer` 基类
- `TopologyJsonSerializer`（`displayMode` 通用字符串）

## 5. 边界与不做

- 不做量程配置 UI（决策 2）
- 不做单位显示（无元数据）
- 不做指针 / 饼图样式配置 UI（保留 API 即可）
- 不实现动画
- 不引入 `GaugeCar` 头文件，仅移植绘制逻辑

## 6. 验证

- 构建 `scripts/build_ninja.bat -t debug -m ETestStudio`
- 手工：拓扑监听器显示模式选「指针表」→ 勾选通道 → 注入 AD/DA 数据观察指针转动与数值更新；`clearData` 归零到量程起点；切换亮/暗主题检查盘面/指针/刻度配色融合
- 检查 `gauge` 序列化往返（通用字符串，预期无需改）
