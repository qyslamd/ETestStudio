# 项目硬件重构方案

> **设计结论：** 项目树的"硬件"不是文件目录，而是当前项目拓扑引用的设备集合。
> 平台设备树（侧边栏）展示"平台有什么硬件"；项目硬件（项目树）展示"项目用了什么硬件"。

---

## 背景

当前项目树（ProjectStructureWidget）有 8 个类别节点，其中"硬件"映射到 `hardware/` 目录。
但这个目录在六层架构中没有对应的概念——硬件属于 HAL 插件层，不是项目文件系统的一部分。
`ProjectManager::createProjectStructure()` 也不会创建 `hardware/` 目录。

此外，`DevicePaletteWidget` 中的设备类型是硬编码的 3 个条目，与 PluginManager 脱节；
`TopologyDevice` 没有 `pluginId` 字段，无法区分同型号多板卡。

## 目标

1. 项目树"硬件"节点 → 从拓扑文件解析设备引用，显示状态
2. DevicePaletteWidget → 从 PluginManager 动态加载设备类型
3. TopologyDevice → 增加 pluginId 字段，全链路贯通
4. 项目树硬件节点优先按 pluginId 精准匹配插件

```
Demo_Project3
├── 协议 (2)
├── 拓扑 (2)
├── 硬件 (3)             ← 不再扫描 hardware/ 目录
│   ├── A429激励设备1 (EPH6272T)  [在线]   ← 颜色标识状态
│   ├── 模拟量采集设备1 (EPH6633A) [未加载] ← 橙色
│   └── 离散量设备1 (EPH5121A)   [在线]   ← 绿色
├── 用例 (2)
└── 脚本 (3)
```

---

## 数据流

### 设备拖入场景

```
PluginManager::loadedPlugins() → 筛选 category=="device"
  │
  ├── DevicePaletteWidget 列表展示（pluginId + deviceType + 显示名）
  │
  ▼
拖拽 → MIME JSON { deviceType, channelCount, direction, functionType, pluginId }
  │
  ▼
TopologyScene::deviceDropped(deviceType, channelCount, direction, functionType, pluginId, pos)
  │
  ▼
TopologyEditorWidget::onDropDevice()
  → TopologyDevice { name, deviceType, pluginId, ... }
  → AddDeviceCommand → TopologyDocument::addDevice()
  │
  ▼
TopologyJsonSerializer 序列化 → .etopo JSON 包含 "pluginId" 字段
```

### 项目树硬件节点刷新

```
拓扑文件 (.etopo)
  │  QJsonDocument 解析 devices[] 数组
  │  提取 {deviceType, name, pluginId}
  ▼
去重后的设备引用列表
  │
  └── PluginManager::plugin(pluginId) 匹配 → 无匹配显示 [未加载]（橙色）
```

---

## 改动清单

### 一、PluginMetaData — 增加字段

**文件：** `src/core/plugin/PluginMetaData.h`

```cpp
struct PluginMetaData {
  // ... 现有字段 ...
  QString device_function;   // 新增，如 "A429"、"AD"、"DISCRETE"
  QString device_direction;  // 新增，如 "Bidirectional"，默认 "Bidirectional"
};
```

在 `PluginManager::parseMetaDataFromLib()`（第 217 行）中，第 235 行 `meta.device_channels = ...` 之后增加：

```cpp
meta.device_function = metaDataObj.value("device_function").toString();
meta.device_direction = metaDataObj.value("device_direction").toString(
    QStringLiteral("Bidirectional"));
```

插件元数据 JSON 格式新增两个可选字段：

```json
{
  "MetaData": {
    "device_type": "EPH6272T",
    "device_channels": 4,
    "device_function": "A429",
    "device_direction": "Bidirectional"
  }
}
```

### 二、TopologyDevice — 增加 pluginId 字段

**文件：** `src/topology/TopologyDocument.h`

```cpp
struct TopologyDevice {
  QString name;
  QString deviceType;
  QString pluginId;       // NEW: 设备插件实例唯一标识，必填
  QPointF position{0, 0};
  QVector<QPair<QString, QString>> properties;
  QVector<TopologyDevicePort> ports;
  QSizeF size{0, 0};
};
```

> **注意：** 所有通过 UI 创建设备的路径（拖拽、右键添加、粘贴）都必须携带有效 `pluginId`。旧版 `.etopo` 文件加载时 `pluginId` 为空，项目树中对应设备显示 [未加载]。

### 三、TopologyJsonSerializer — 序列化 pluginId

**文件：** `src/topology/TopologyJsonSerializer.cpp`

序列化时增加：

```cpp
obj["pluginId"] = dev.pluginId;
```

反序列化时读取：

```cpp
dev.pluginId = obj["pluginId"].toString();
```

> **说明：** `pluginId` 是必填字段，序列化时不检查空值。旧版 `.etopo` 无此字段时加载为空字符串。

### 四、DevicePaletteWidget — 从 PluginManager 动态加载

**文件：** `src/topology/DevicePaletteWidget.h` — 删除 `DeviceEntry` 结构体；新增 `addMonitorEntry()` 声明。
**文件：** `src/topology/DevicePaletteWidget.cpp`

**删除：** 硬编码的 `kDeviceTypes[]` 数组、`kDeviceTypeCount` 常量。`DeviceEntry` 结构体（`DevicePaletteWidget.h` 第 14-20 行）仅被 `kDeviceTypes[]` 引用，一并删除。

**保留并提取：** `kMonitorTypes[]` 和 `MonitorEntry` 结构体保留，但提取到独立的 `addMonitorEntry()` 方法中（监视器是拓扑概念，不从插件加载）。注意覆盖所有 `Qt::UserRole + N` 数据角色，使 `startDrag()` 可以统一通过 item data 读取。

**新增头文件：** `DevicePaletteWidget.cpp` 需要 `#include "plugin/PluginManager.h"`。

**新增：** `populateDeviceTypes()` 直接访问 PluginManager：

```cpp
void DevicePaletteWidget::populateDeviceTypes() {
  device_list_->clear();

  // 从 PluginManager 加载设备类型
  auto& pm = PluginManager::instance();
  for (const auto& meta : pm.loadedPlugins()) {
    if (meta.category != "device") continue;
    if (meta.device_type.isEmpty()) continue;

    auto* item = new QListWidgetItem(device_list_);
    // 显示名：优先用 meta.name，回退到 device_type
    QString display = meta.name.isEmpty()
        ? meta.device_type
        : QStringLiteral("%1 (%2ch)").arg(meta.name).arg(meta.device_channels);
    item->setText(display);
    item->setData(Qt::UserRole,     meta.device_type);      // deviceType
    item->setData(Qt::UserRole + 1, false);                  // isMonitor
    item->setData(Qt::UserRole + 2, meta.device_channels);   // channelCount
    item->setData(Qt::UserRole + 3, meta.id);                // pluginId

    // 方向：从 meta 读取，默认 Bidirectional
    int direction = static_cast<int>(TopologyPort::Direction::Bidirectional);
    if (meta.device_direction == "Input")
      direction = static_cast<int>(TopologyPort::Direction::Input);
    else if (meta.device_direction == "Output")
      direction = static_cast<int>(TopologyPort::Direction::Output);
    item->setData(Qt::UserRole + 4, direction);              // direction

    // 功能类型：从 meta.device_function 解析，空则回退到 CUSTOM
    FunctionType ft = stringToFunctionType(meta.device_function.isEmpty()
        ? QStringLiteral("CUSTOM") : meta.device_function);
    item->setData(Qt::UserRole + 5, static_cast<int>(ft));   // functionType
    item->setFlags(item->flags() | Qt::ItemIsDragEnabled);
  }

  // 监视器（保留硬编码，addMonitorEntry 内部会添加分隔线）
  addMonitorEntry();
}
```

已有 `stringToFunctionType()`（TopologyDocument.h 第 30 行），直接使用即可。

**提取 `addMonitorEntry()` 方法**（监视器不属于 PluginManager 加载的硬件设备，保持硬编码）：

```cpp
void DevicePaletteWidget::addMonitorEntry() {
  static const MonitorEntry kMonitorTypes[] = {
      {"Monitor-4CH", "Monitor-4CH (4通道监听器)", 4},
  };
  static const int kMonitorTypeCount =
      sizeof(kMonitorTypes) / sizeof(kMonitorTypes[0]);

  if (kMonitorTypeCount > 0) {
    auto* sep = new QListWidgetItem(QStringLiteral("─── 监听器 ───"));
    sep->setFlags(sep->flags() & ~Qt::ItemIsSelectable);
    sep->setForeground(QColor(140, 140, 140));
    device_list_->addItem(sep);

    for (int i = 0; i < kMonitorTypeCount; ++i) {
      const auto& entry = kMonitorTypes[i];
      auto* item = new QListWidgetItem(entry.displayName);
      item->setData(Qt::UserRole, entry.deviceType);
      item->setData(Qt::UserRole + 1, true);                    // isMonitor
      item->setData(Qt::UserRole + 2, entry.channelCount);      // channelCount
      item->setData(Qt::UserRole + 3, QString());               // pluginId（空）
      item->setData(Qt::UserRole + 4,
          static_cast<int>(TopologyPort::Direction::Bidirectional)); // direction
      item->setData(Qt::UserRole + 5,
          static_cast<int>(FunctionType::CUSTOM));              // functionType
      item->setToolTip(QStringLiteral("%1\n拖放至画布添加监听器").arg(
          entry.displayName));
      item->setFlags(item->flags() | Qt::ItemIsDragEnabled);
      device_list_->addItem(item);
    }
  }
}
```

监视器条目设置与设备条目相同的数据角色，保证 `startDrag()` 可以统一通过 `item->data()` 读取 MIME 数据。

**插件热加载刷新：** Palette 需要响应 PluginManager 信号，在插件动态加载/卸载时刷新列表：

```cpp
// DevicePaletteWidget 构造函数或首次 populate 时连接
auto& pm = PluginManager::instance();
connect(&pm, &PluginManager::pluginLoaded,
        this, &DevicePaletteWidget::populateDeviceTypes);
connect(&pm, &PluginManager::pluginUnloaded,
        this, &DevicePaletteWidget::populateDeviceTypes);
```

`populateDeviceTypes()` 可多次调用——每次调用清除列表后重新查询 PluginManager。

**调用时机：** `populateDeviceTypes()` 应**延迟到 Palette 首次显示时**调用（通过 `showEvent` 或 `TopologyEditorWidget` 场景初始化完成后触发），确保 PluginManager 已完成插件加载。不在构造函数中调用。

**Mock 插件元数据补充：** 现有 mock 插件未声明 `device_function`，会导致 palette 中设备功能类型为空。需补充：

| 插件 | device_type | 补 device_function |
|---|---|---|
| mock_ad | "ad" | "AD" |
| mock_can | "can" | "CAN" |

```cpp
// MockADPlugin 构造函数中增加
meta_.device_function = "AD";
// MockCANPlugin 构造函数中增加
meta_.device_function = "CAN";
```

### 五、拖拽 MIME 数据 — 增加 pluginId，改用 item data 统一读取

**文件：** `src/topology/DevicePaletteWidget.cpp` — `startDrag()`

`kDeviceTypes[]` 删除后，原通过数组遍历查询 MIME 数据的逻辑（第 75-83 行）不再可用。统一改用 `item->data()` 角色值读取，设备与监视器共用同一路径：

```cpp
void DevicePaletteWidget::DeviceListWidget::startDrag(
    Qt::DropActions supportedActions) {
  auto items = selectedItems();
  if (items.isEmpty())
    return;

  auto* item = items.first();

  QJsonObject obj;
  obj["deviceType"]    = item->data(Qt::UserRole).toString();
  obj["isMonitor"]     = item->data(Qt::UserRole + 1).toBool();
  obj["channelCount"]  = item->data(Qt::UserRole + 2).toInt();
  obj["pluginId"]      = item->data(Qt::UserRole + 3).toString();  // NEW
  obj["direction"]     = item->data(Qt::UserRole + 4).toInt();
  obj["functionType"]  = item->data(Qt::UserRole + 5).toInt();

  auto* mime = new QMimeData();
  mime->setData(QLatin1String(kTopologyDeviceMime),
                QJsonDocument(obj).toJson(QJsonDocument::Compact));

  auto* drag = new QDrag(this);
  drag->setMimeData(mime);
  drag->exec(supportedActions);
}
```

不再需要引用 `kDeviceTypeCount`、`kDeviceTypes[]`、`kMonitorTypeCount`、`kMonitorTypes[]`。

### 六、TopologyScene::deviceDropped — 增加 pluginId 参数

**文件：** `src/topology/TopologyScene.h`

```cpp
signals:
  void deviceDropped(const QString& deviceType,
                     int channelCount,
                     int direction,
                     int functionType,
                     const QString& pluginId,    // NEW
                     const QPointF& scenePos);
```

**文件：** `src/topology/TopologyScene.cpp` — `dropEvent()`

```cpp
emit deviceDropped(
    obj["deviceType"].toString(),
    obj["channelCount"].toInt(),
    obj["direction"].toInt(),
    obj["functionType"].toInt(),
    obj["pluginId"].toString(),      // NEW
    event->scenePos());
```

### 七、TopologyEditorWidget::onDropDevice — 存储 pluginId

**文件：** `src/topology/TopologyEditorWidget.cpp`

```cpp
void TopologyEditorWidget::onDropDevice(const QString& deviceType,
                                        int channelCount,
                                        int direction,
                                        int functionType,
                                        const QString& pluginId,    // NEW
                                        const QPointF& scenePos) {
  TopologyDevice dev;
  dev.deviceType = deviceType;
  dev.pluginId = pluginId;            // NEW
  dev.name = QStringLiteral("%1_%2").arg(deviceType).arg(n, 2, 10, QChar('0'));
  // ...
}
```

同时需要检查 `onAddDevice()`（右键菜单"添加设备"）和 `onAddDeviceFromTemplate()` 路径：

- **`onAddDevice()`：** 当前硬编码 "EPH6272T"。改为查询 PluginManager，取第一个 `category=="device"` 的插件创建设备（`deviceType` + `pluginId`）。若无可用插件，**什么也不做**（不创建无对应插件的设备）。
- **`onAddDeviceFromTemplate()`：** 从 .dvt 文件加载后，查询 PluginManager 按 `deviceType` 匹配第一个可用插件填充 `pluginId`。若无匹配，**报错提示用户先加载对应插件**，不创建设备。

**`TopologyEditorWidget::initSignals()` 更新：** `deviceDropped` 信号参数列表变更后，原有的 `connect()` 调用签名不匹配。需要同步更新连接：

```cpp
connect(scene_, &TopologyScene::deviceDropped,
        this, &TopologyEditorWidget::onDropDevice);
```

如果使用新式信号槽语法（`SIGNAL()`/`SLOT()` 宏），参数顺序变化不会引起编译错误但仍需验证；推荐使用 `&` 成员函数指针语法，参数个数不匹配会直接编译报错。

### 八、Copy/Paste — 补充 pluginId

**文件：** `src/topology/TopologyEditorWidget.cpp`

`onCopy()` 中设备 JSON 构造需增加 `pluginId`（第 1143 行附近）：

```cpp
obj["name"] = d->name;
obj["deviceType"] = d->deviceType;
obj["pluginId"] = d->pluginId;            // NEW
```

`onPaste()` 中设备反序列化需读取 `pluginId`（第 1266 行附近）：

```cpp
dev.deviceType = obj["deviceType"].toString();
dev.pluginId = obj["pluginId"].toString(); // NEW
```

### 九、ProjectStructureWidget — 项目树硬件节点

**文件：** `src/app/ProjectStructureWidget.h`、`src/app/ProjectStructureWidget.cpp`

**新增头文件包含：**
```cpp
#include "plugin/PluginManager.h"
#include "plugin/IDevicePlugin.h"
```

**defaultCategories() 修改：** 删除 `"hardware"` 分类条目（第 441-443 行）。硬件不再映射到 `hardware/` 目录，改为由 `buildTree()` 在所有 filesystem 节点之后追加一个计算节点。

**buildTree() 修改：** 在完成所有分类节点扫描（包括"其他文件"）后，追加硬件节点：

```cpp
// ── 硬件节点（从拓扑文件解析，非文件系统）──
auto* hwItem = createCategoryItem(
    {QStringLiteral("hardware"), QStringLiteral("硬件"),
     QString(), QStringLiteral("hardware"), QString(), QString()}, 0);
// 先创建空的硬件节点，后续由 refreshHardwareDevices() 填充设备子项
QFont hwFont = hwItem->font();
hwFont.setItalic(true);
hwItem->setFont(hwFont);
hwItem->setToolTip(QStringLiteral(
    "项目拓扑文件中引用的硬件设备列表\n"
    "平台插件加载后自动匹配设备状态"));
root_item_->appendRow(hwItem);
```

**`hardware/` 目录不再被 `defaultCategories()` 收集，但 `buildTree()` 的 `skipPrefixes` 机制通过 `defaultCategories()` 驱动（第 530-533 行）。为防止项目残留 `hardware/` 目录中的文件泄漏到"其他文件"节点，需在 `skipPrefixes` 构建后单独追加排除路径：**

```cpp
// 在 buildTree() 中，skipPrefixes 构建循环之后增加
skipPrefixes.append(
    QDir(project_path_).absoluteFilePath(QStringLiteral("hardware/"))
        .toLower() + QStringLiteral("/"));
```

> 注意：`temp/projects/Demo_Project3/hardware/` 在实施时会被删除（见文件清单），但即使其他项目模板有遗留目录也不会误显。

**refreshHardwareDevices() — 新增方法：** 解析项目所有 `.etopo` 文件，提取设备引用列表，对比 PluginManager 显示状态：

- 遍历项目 `topology/` 目录下的所有 `.etopo` 文件
- 用 `QJsonDocument` 解析 `devices[]` 数组，提取每个设备的 `{name, deviceType, pluginId}`
- 去重（按 `name` 去重，同一设备在多拓扑文件中只显示一次）
- 匹配逻辑：仅通过 `PluginManager::plugin(pluginId)` 精准匹配；`pluginId` 为空或无匹配时显示 [未加载]
- 更新硬件节点的子项，显示状态颜色

调用时机：
- `buildTree()` 末尾调用一次（项目打开时）
- 拓扑文件变更时（`directoryContentChanged` 中判断目录为 `topology/` 时触发）
- 插件加载/卸载时（通过 `PluginManager::pluginLoaded/pluginUnloaded` 信号）

```cpp
void ProjectStructureWidget::connectHardwareRefresh() {
  // 拓扑文件变更时刷新
  connect(this, &ProjectStructureWidget::directoryContentChanged,
          this, [this](const QString& dirPath) {
            if (dirPath.endsWith(QStringLiteral("/topology")) ||
                dirPath.endsWith(QStringLiteral("\\topology"))) {
              refreshHardwareDevices();
            }
          });
  // 插件加载/卸载时刷新
  auto& pm = PluginManager::instance();
  connect(&pm, &PluginManager::pluginLoaded,
          this, &ProjectStructureWidget::refreshHardwareDevices);
  connect(&pm, &PluginManager::pluginUnloaded,
          this, &ProjectStructureWidget::refreshHardwareDevices);
}
```

**调用位置：** `connectHardwareRefresh()` 在 `ProjectStructureWidget::initSignals()` 末尾调用一次。

设备状态颜色规则：
| 状态 | 颜色 |
|------|------|
| 已匹配（PluginManager::plugin() 返回非空）且 IDevicePlugin::status() 正常 | `#4CAF50`（绿色） |
| 已匹配但设备状态异常 | `#FF9800`（橙色） |
| 无匹配 | `#999999`（灰色），后缀 `[未加载]` |

### 十、MainWindow 联动与导航信号链

**文件：** `src/app/main_window.cpp`

**拓扑文件变更刷新硬件节点：**
- `ProjectStructureWidget::directoryContentChanged` 信号已在九节通过 `connectHardwareRefresh()` 连接到 `refreshHardwareDevices()`，拓扑目录变更时硬件节点自动更新。
- `MainWindow` 中现有的拓扑文件变更监听（如 `TopologyEditorWidget` 的文档修改信号）无需额外处理。

**右键/双击导航到平台设备树：**

```cpp
// ProjectStructureWidget 新增信号
signals:
  void hardwareDeviceNavigateRequested(const QString& deviceType,
                                       const QString& pluginId);

// ProjectStructureWidget 右键菜单/双击处理
void ProjectStructureWidget::onHardwareDeviceContextMenu(
    const QString& deviceType, const QString& pluginId) {
  // 发出导航信号
  emit hardwareDeviceNavigateRequested(deviceType, pluginId);
}

// MainWindow 连接导航
void MainWindow::initHardwareNavigation() {
  connect(project_structure_widget_,
      &ProjectStructureWidget::hardwareDeviceNavigateRequested,
      this, [this](const QString& deviceType, const QString& pluginId) {
    // 1. 侧边栏切换到平台设备树页面
    sidebar_->switchToPage(SidebarWidget::HardwareTree);
    // 2. 高亮对应的设备类型
    hardware_tree_widget_->highlightDeviceType(deviceType, pluginId);
  });
}
```

导航涉及的组件：`ProjectStructureWidget`（右键/双击）→ `MainWindow`（中转）→ `SidebarWidget`（切换页面）→ `HardwareTreeWidget`（高亮）。

---

## 涉及文件清单（完整）

| 文件 | 改动 |
|---|---|
| `src/core/plugin/PluginMetaData.h` | 新增 `device_function`、`device_direction` 字段 |
| `src/core/plugin/PluginManager.cpp` | `parseMetaDataFromLib()` 中解析新增字段 |
| `src/topology/TopologyDocument.h` | `TopologyDevice` 加 `pluginId` |
| `src/topology/TopologyJsonSerializer.cpp` | 序列化/反序列化 `pluginId` |
| `src/topology/DevicePaletteWidget.h` | 删除 `DeviceEntry` 结构体；新增 `addMonitorEntry()` 声明 |
| `src/topology/DevicePaletteWidget.cpp` | 删除 `kDeviceTypes[]`/`kDeviceTypeCount`；重写 `populateDeviceTypes()` 从 PluginManager 加载；提取 `addMonitorEntry()`；`startDrag()` 改用 item data 角色统一读取 MIME 数据；增加 `plugin/PluginManager.h` 包含 |
| `src/topology/TopologyScene.h` | `deviceDropped` 信号加 pluginId 参数 |
| `src/topology/TopologyScene.cpp` | `dropEvent` 传递 pluginId |
| `src/topology/TopologyEditorWidget.cpp` | `onDropDevice` 接收 pluginId；`onAddDevice` 查询 PluginManager；`initSignals()` 更新 connect |
| `examples/plugins/mock_ad_device/MockADPlugin.cpp` | 补充 `meta_.device_function` |
| `examples/plugins/mock_can_device/MockCANPlugin.cpp` | 补充 `meta_.device_function` |
| `src/app/ProjectStructureWidget.h` | 新增 `refreshHardwareDevices()`、`connectHardwareRefresh()`、`hardwareDeviceNavigateRequested` 信号 |
| `src/app/ProjectStructureWidget.cpp` | `defaultCategories()` 移除 hardware 条目；`buildTree()` 末尾追加硬件计算节点；`refreshHardwareDevices()` 实现；`connectHardwareRefresh()` 信号连接；增加 `plugin/PluginManager.h`、`plugin/IDevicePlugin.h` 包含 |
| `src/app/HardwareTreeWidget.h` | 新增 highlightDeviceType |
| `src/app/HardwareTreeWidget.cpp` | 实现 highlightDeviceType |
| `src/app/main_window.cpp` | 拓扑变更刷新 + 导航联动 |
| `temp/projects/Demo_Project3/hardware/` | 删除 |

不需要改动的文件：

| 文件 | 原因 |
|---|---|
| `src/app/SidebarWidget.h/.cpp` | 页面切换逻辑不变 |
| `src/topology/UndoCommands.h/.cpp` | AddDeviceCommand 已存整个 TopologyDevice 副本 |
| `src/topology/topology_items.h/.cpp` | DeviceItem 通过 device_index_ 读 TopologyDevice，改结构体自动继承 |

---

## 向后兼容性

| 场景 | 行为 |
|---|---|
| 旧版 palette 代码 | 全部替换，不存在兼容问题 |
| PluginMetaData 无新字段 | `device_function` 为空 → functionType 默认为 Custom；`device_direction` 为空 → 默认 Bidirectional |
| 无插件加载 | Palette 显示空列表（只保留监视器），项目硬件显示 [未加载] |

---

## 验证步骤

1. 构建并运行，打开 Demo_Project3
2. DevicePaletteWidget 显示从 PluginManager 加载的设备类型（不再是 3 个硬编码条目）
3. 从 palette 拖入设备到拓扑 → 保存 → 打开 `.etopo` 确认 `pluginId` 已写入
4. 项目树"硬件"节点显示拓扑引用的设备列表，带状态和颜色
5. 拓扑目录为空时，"硬件 (0)" 节点有 tooltip 提示
6. 新建/编辑/保存拓扑文件，硬件节点自动刷新
7. 右键设备节点 → "跳转到设备树" → 侧边栏切换到平台设备树
8. 双击设备节点 → 同样跳转到平台设备树
9. 切换主题（default/vscode），显示正常
