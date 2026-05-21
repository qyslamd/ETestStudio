# PropertyPanelWidget 性能优化计划

## 背景

96 通道设备选中时属性面板打开缓慢。根因是设备端口表使用 QTableWidget + `setCellWidget` 为每行创建 QComboBox（2个/行），96 行即 192 个常驻 ComboBox。256 通道设备会放大到 512 个，延迟更显著。

## 优化方案

### 核心思路：QTableView + QStyledItemDelegate

将 `device_port_table_` 从 QTableWidget 替换为 QTableView，方向列和功能类型列使用自定义 Delegate，ComboBox 只在编辑时由 Delegate 创建，非编辑状态只显示文本。

**新增文件：**
- `src/topology/ComboBoxDelegate.h` / `.cpp` — 通用 ComboBox Delegate，构造时传入选项列表，可复用于方向和功能类型两列

**修改文件：**
- `src/topology/PropertyPanelWidget.h` — 替换成员类型，添加 Delegate 实例
- `src/topology/PropertyPanelWidget.cpp` — 重构设备端口表的数据填充/读取逻辑
- `src/topology/CMakeLists.txt` — 添加新源文件

### 数据模型

QStandardItemModel，3 列：端口名称（字符串）、方向（int）、功能类型（int）。方向/功能类型存 int 值而非显示文本，避免序列化/反序列化开销。

### 数据流对比

| 步骤 | 之前 (QTableWidget) | 之后 (QTableView + Delegate) |
|------|-------------------|---------------------------|
| 加载数据 | 创建 96 个 QTableWidgetItem + 96×2 个 QComboBox，`setCellWidget` 触发 192 次布局 | 创建 3×96 = 288 个 QStandardItem 设置 data，无 ComboBox 创建 |
| 编辑方向 | ComboBox 常驻显示 | 点击单元格 → Delegate 创建 ComboBox → 选择 → 销毁 |
| 功能类型 | ComboBox 常驻显示 | 同上 |
| 保存数据 | 遍历行，`cellWidget(r, 1)` 转 QComboBox 获取值 | 遍历行，`model->index(r, 1).data(Qt::DisplayRole)` |
| 行数扩展 | 每行增加 2 个 ComboBox 常驻内存 | 仅增加 3 个 QStandardItem，内存与行数接近线性 |

### 脏标记优化

在上述重构基础上同步引入 `device_dirty_` 标记：

- `onDevicePortDirectionChanged(int)` / `onDevicePortFunctionTypeChanged(int)` 置脏（已有空操作，加入 `= true`）
- `onAddDevicePortRow()` / `onRemoveDevicePortRow()` 置脏
- 连接 `model->itemChanged` → 置脏（端口名称编辑、属性键/值编辑）
- `showPropertiesFor()` 离开设备页时：若 `!device_dirty_` 跳过 apply + documentChanged，直接 return
- `applyDevicePorts()` / `applyDeviceProperties()` 开头检查 `!device_dirty_` 直接 return
- 保存成功后 `device_dirty_ = false`

### 涉及的源文件

| 文件 | 操作 |
|------|------|
| `src/topology/ComboBoxDelegate.h` | 新建 |
| `src/topology/ComboBoxDelegate.cpp` | 新建 |
| `src/topology/PropertyPanelWidget.h` | 替换成员类型，添加脏标记 |
| `src/topology/PropertyPanelWidget.cpp` | 重构设备端口表读写逻辑 |
| `src/topology/CMakeLists.txt` | 添加新源文件 |

## 验证

1. 编译通过
2. 选中 96 通道设备 → 面板秒开
3. 编辑端口方向/功能类型 → ComboBox 正常弹出
4. 在两个设备间反复切换（无编辑）→ 无卡顿
5. 编辑后切换 → 编辑内容正确保存，撤销正常
