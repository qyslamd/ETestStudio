# topology-demo — 测试系统拓扑编辑器

基于 Qt5/QGraphicsView 的测试系统拓扑图编辑器，仿凯云 ETest 拓扑编辑功能。

## 构建

```bash
# 使用 ninja 构建
scripts\build_ninja.bat
scripts\run_topology-demo.bat
```

## 功能列表

### 图形编辑器

| 功能 | 说明 |
|------|------|
| 视口平移 | 鼠标中键拖拽平移画布 |
| 滚轮缩放 | 滚轮缩放，范围 0.1x ~ 10x |
| 橡皮筋框选 | 默认拖拽模式为 RubberBandDrag |
| 右键菜单 | 空白处右键 → 添加 UUT / 添加设备；选中项右键 → 删除 / 另存为模板 |

### 拓扑元素

| 元素 | 说明 |
|------|------|
| **UUT 产品** | 蓝色矩形块，左侧排列端口，支持拖拽移动 |
| **设备** | 橙色圆角矩形块，右侧排列端口，支持拖拽移动 |
| **端口** | 彩色圆点 + 引出线，UUT 上为蓝色/绿色/品红，设备上相同配色 |
| **连线** | 贝塞尔曲线连接 UUT 端口与设备端口，中点显示信号流向箭头 |

### 方向系统

每个端口（UUT 和设备端口）支持三种方向：

| 方向 | 颜色 | 端口端点标记 | 连线中点箭头 |
|------|------|-------------|-------------|
| Input | 蓝 `#4285F4` | — | 实心三角指向 UUT |
| Output | 绿 `#34A853` | — | 实心三角指向设备 |
| Bidirectional | 品红 `#FF00FF` | `<---->` | 描边 `<---->` 沿切线 |

右下角图例实时显示三种方向的颜色标识。

### 动态大小

- **UutItem**：高度根据端口数量自动计算，最小 60px，每端口 20px
- **DeviceItem**：高度根据端口数量自动计算，最小 50px，每端口 20px

### 拖拽连线

- 从 UUT 端口拖拽 → 吸附到设备端口 → 创建连线
- 从设备端口拖拽 → 吸附到 UUT 端口 → 创建连线
- 拖拽过程中显示虚线预览
- 移动元素时连线自动跟随更新

### 属性面板

| 页面 | 可编辑属性 |
|------|-----------|
| UUT | 名称 |
| 端口 | 名称、方向（Input/Output/Bidirectional）、允许设备类型（只读） |
| 设备 | 名称、设备类型（只读）、属性表（键/值，可增删行） |
| 设备端口（表格） | 名称、方向（Input/Output/Bidirectional）、功能类型 |
| 设备端口（单独） | 端口名称、方向、功能类型 |
| 连线 | 源 UUT/端口、目标设备名称|

### 文件操作

- 新建 / 打开 / 保存 / 另存为 `.json` 格式的拓扑文件
- 支持设备另存为 `.dvt` 模板文件

### 默认示例数据

| UUT | 端口 |
|-----|------|
| ISI-01 | A429_CH1(Bidirectional)、A429_CH2(Bidirectional)、A429_CH3(Bidirectional)、离散量 |
| ISI-02 | A429_CH1(Bidirectional) |

| 设备 | 端口 |
|------|------|
| EPH6272T-00 | ch0(A429, Bidirectional)、ch1(A429, Bidirectional) |
| EPH6272T-01 | ch0(A429, Bidirectional)、ch1(A429, Bidirectional) |
| EPH5121A-00 | ch0(DISCRETE, Bidirectional) |

## 项目结构

```
topology-demo/
├── main.cpp                          # 入口
├── CMakeLists.txt
├── TopologyDocument.h/.cpp           # 数据模型
├── TopologyJsonSerializer.h/.cpp     # JSON 序列化
├── topology_items.h/.cpp             # 图形项（UutItem/PortItem/DeviceItem/DevicePortItem/ConnectionItem/LegendItem）
├── TopologyScene.h/.cpp              # 场景管理 + 交互逻辑
├── TopologyView.h/.cpp               # 视图 + 缩放/平移/右键菜单
├── TopologyEditorWidget.h/.cpp       # 主窗口 + 菜单/工具栏
├── PropertyPanelWidget.h/.cpp        # 属性面板
└── DeviceTemplateManager.h/.cpp      # 设备模板
```
