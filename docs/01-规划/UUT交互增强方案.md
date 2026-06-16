# UUT 交互增强：右键菜单端口操作 + 属性面板扩展

## 动机

当前 UUT（被测设备）存在两个交互缺陷：

1. **右键菜单功能缺失** — UUT 只有"删除 UUT"，端口不可通过右键增减。端口数量完全由初始数据决定，用户无法在运行时调整，不符合编辑器的基本预期。
2. **属性面板过于简陋** — 仅一个名称字段。`TopologyProduct` 数据结构已有 `size`、`ports` 等字段，但 UI 没有暴露，等于有数据没操作入口。

## 现状分析

### 数据层

`TopologyProduct` 已有字段：
```cpp
struct TopologyProduct {
  QString name;
  QVector<TopologyPort> ports;  // 含 name/direction/functionType/positionHint/portStyle
  QPointF position{0, 0};
  QSizeF size{0, 0};            // 宽高，0=自动
};
```

`TopologyDocument` 已有端口相关 API（但仅限 Device，无 Product 端口增删）：
- `addDevicePort(int deviceIndex, const TopologyDevicePort&)`
- `removeDevicePort(int deviceIndex, int portIndex)`

**缺失**：Product 端口的 `addPort`/`removePort` API 和对应 UndoCommand。

### 右键菜单

| 对象 | 当前菜单项 |
|------|-----------|
| PortItem | 端口样式（圆形/三角形）、删除端口 |
| UutItem | 删除 UUT |
| DeviceItem | 删除设备、另存为模板 |

PortItem 已有"删除端口"功能，但 UutItem 上没有"添加端口"。

### 属性面板

| 对象 | 当前属性字段 |
|------|-------------|
| UutItem | 名称 |
| PortItem | 名称、方向、允许设备类型、功能类型 |
| DeviceItem | 名称、设备类型、自定义属性表、端口表 |

Device 的属性面板已经包含完整的端口管理表格（增删改），UUT 则完全没有。

### 图形层

`TopologyBlockItem` 已支持 8 方向 resize handle，`UutItem::onResizeFinished` 已推送 `ResizeItemCommand`。但 resize 后的尺寸没有在属性面板中暴露为可编辑字段。

## 设计

### 1. 右键菜单：UUT 添加端口

在 `TopologyView::contextMenuEvent` 的 `uut` 分支增加菜单项：

```
UUT 右键菜单
├── 添加端口
└── 删除 UUT
```

**"添加端口"** 行为：
- 弹出 `QInputDialog` 输入端口名称（默认 "Port_N"，N 为当前端口数+1）
- 创建 `TopologyPort` 默认值：direction=Input, functionType=CUSTOM
- 通过新增的 `AddProductPortCommand`（UndoCommand）追加到 `TopologyProduct::ports`
- 触发场景重建 → `layoutPorts()` 更新图形

**与 PortItem 右键"删除端口"的配合**：
- PortItem 的"删除端口"已存在，但当前只是 `setPortStyle` 相关。需确认它是否真正从文档中移除端口。如果当前只是视觉删除，需要补全为从 `TopologyProduct::ports` 移除，走 `RemoveProductPortCommand`。

### 2. 数据层：Product 端口增删 API

在 `TopologyDocument` 中新增：

```cpp
// TopologyDocument.h
void addProductPort(int productIndex, const TopologyPort& port);
void removeProductPort(int productIndex, int portIndex);
```

新增信号：
```cpp
void productPortAdded(int productIndex, int portIndex);
void productPortRemoved(int productIndex, int portIndex);
```

### 3. Undo 命令

新增两个 UndoCommand（参考已有 `RemoveDevicePortCommand` 模式）：

```cpp
// UndoCommands.h
class AddProductPortCommand : public QUndoCommand {
 public:
  AddProductPortCommand(TopologyDocument* doc, int productIndex,
                        const TopologyPort& port, QUndoCommand* parent = nullptr);
  void undo() override;
  void redo() override;
 private:
  TopologyDocument* doc_;
  int product_index_;
  TopologyPort port_;
  int port_index_ = -1;  // redo 时记录实际插入位置
};

class RemoveProductPortCommand : public QUndoCommand {
 public:
  RemoveProductPortCommand(TopologyDocument* doc, int productIndex,
                           int portIndex, QUndoCommand* parent = nullptr);
  void undo() override;
  void redo() override;
 private:
  TopologyDocument* doc_;
  int product_index_;
  int port_index_;
  TopologyPort port_;         // 保存被删端口用于 undo
  QVector<ConnEntry> saved_connections_;  // 级联删除的连线
};
```

**关键**：`RemoveProductPortCommand` 必须级联处理该端口上的连线，与 `RemoveProductCommand` 的逻辑一致——删除端口前查找并保存所有引用该端口的 `TopologyConnection`，undo 时一并恢复。

### 4. 属性面板：UUT 页面扩展

将 `buildUutPage()` 从单字段扩展为分组布局：

```
UUT 属性面板
├── 基本信息
│   └── 名称 [QLineEdit]
├── 尺寸
│   ├── 宽度 [QSpinBox, 0=自动]
│   └── 高度 [QSpinBox, 0=自动]
└── 端口列表
    ├── [QTableWidget: 名称 | 方向 | 功能类型]
    ├── [+ 添加端口] [- 删除端口]
    └── 双击单元格编辑
```

#### 4.1 尺寸字段

- `QSpinBox`，范围 0-9999，0 表示自动（由端口数量决定高度）
- 值来源：`TopologyProduct::size.width()` / `size.height()`
- 编辑完成时推送 `PropertyCommand` 修改 `prod->size`，同时通知 `UutItem` 更新 `block_width_` / `block_height_`

#### 4.2 端口列表

参考 Device 页面的端口表格实现（`device_port_view_` + `QStandardItemModel`），为 UUT 页面建立类似结构：

- `QTableWidget` 三列：名称(QString)、方向(Input/Output/Bidirectional)、功能类型(A429/AD/DA/...)
- 行数据来源：`TopologyProduct::ports`
- `+` 按钮触发添加端口（与右键菜单共享同一逻辑，走 `AddProductPortCommand`）
- `-` 按钮删除选中行（走 `RemoveProductPortCommand`）
- 双击单元格编辑后推送 `PropertyCommand` 更新对应端口属性

#### 4.3 背景图

暂不实现。理由：
- `TopologyProduct` 数据结构中没有背景图字段，需要先扩展数据模型 + 序列化
- 背景图的渲染路径（QPixmap 加载、缩放策略、内存管理）需要单独设计
- 可作为后续迭代，不影响本次核心功能

## 改动文件清单

| 文件 | 改动内容 |
|------|---------|
| `src/topology/TopologyDocument.h` | 新增 `addProductPort`/`removeProductPort` 方法声明 + 信号 |
| `src/topology/TopologyDocument.cpp` | 实现 `addProductPort`/`removeProductPort` |
| `src/topology/UndoCommands.h` | 新增 `AddProductPortCommand`/`RemoveProductPortCommand` |
| `src/topology/UndoCommands.cpp` | 实现两个新命令 |
| `src/topology/TopologyView.cpp` | UUT 右键菜单增加"添加端口"；修正 PortItem"删除端口"走 UndoCommand |
| `src/topology/PropertyPanelWidget.h` | 新增 UUT 页面控件成员：宽度/高度 SpinBox、端口表格、增删按钮 |
| `src/topology/PropertyPanelWidget.cpp` | 重写 `buildUutPage()`；新增 UUT 端口表格 CRUD slot |
| `src/topology/TopologyScene.cpp` | 监听 `productPortAdded`/`productPortRemoved` 信号，刷新场景 |
| `src/topology/TopologyJsonSerializer.cpp` | 无需改动（已序列化 `TopologyProduct::ports`） |

## 实施顺序

1. **数据层**：`TopologyDocument` 添加 `addProductPort`/`removeProductPort` + 信号
2. **Undo 命令**：`AddProductPortCommand` / `RemoveProductPortCommand`
3. **右键菜单**：TopologyView UUT 分支增加"添加端口"
4. **场景联动**：TopologyScene 监听新信号，触发 `layoutPorts()` 重建
5. **属性面板**：扩展 `buildUutPage()`，添加尺寸编辑 + 端口表格
6. **端口表格 CRUD**：增删改与 UndoCommand 串联

## 验证要点

- 右键 UUT → 添加端口 → 场景中立即出现新端口图形
- 撤销 → 端口消失；重做 → 端口恢复
- 删除端口 → 关联连线级联删除 → 撤销 → 连线恢复
- 属性面板修改宽度/高度 → 图形实时更新；0 值恢复自动高度
- 属性面板端口表格增/删/编辑 → 场景同步、可撤销
- 保存/加载 .etopo 文件后端口数据完整
