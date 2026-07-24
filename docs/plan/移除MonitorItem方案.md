# 移除 MonitorItem 场景图元方案

## 问题陈述

当前在连线上创建监听器后，场景中会生成一个 `MonitorItem` 方块（带 `MonitorPortItem` 端口），供用户在场景中点击选中以查看和编辑监听器属性。随着架构从 tap 模式演进到 connectionId 模式，该图元的功能已退化为单一的"属性面板入口"，且存在以下问题：

1. **场景杂乱**：监听器方块漂浮在连线附近，用户不清楚其用途
2. **数据冗余**：`TopologyMonitor` 的 `position`/`size` 仅用于 MonitorItem 的定位和大小，序列化到 `.etopo` 中无实际意义
3. **废弃组件残留**：`MonitorPortItem`、`MoveMonitorCommand`、`ResizeItemCommand` monitor 分支均只为 MonitorItem 服务
4. **用户困惑**：新增拓扑时必须拖动/调整它们的位置，增加编辑负担

## 现状架构

### 当前链路

```
右键连线 → 添加监听器
  → AddMonitorCommand → TopologyMonitor { name, connectionId, position, size, displayMode }
  → TopologyScene::addMonitorItem() → MonitorItem (场景方块)
  → TopologyScene::updateMonitorBadges() → badge 图标 (连线上)

点击 badge → ConnectionItem::mousePressEvent
  → findMonitorItem(monitor_index_) → monitorItem->setSelected(true)
    → scene_->selectionChanged → property_panel_->showPropertiesFor(monitorItem)
      → 显示 monitor 属性面板

大纲树点击监听器 → onOutlineNavigate case 5
  → findMonitorItem(mainIndex) → target->setSelected(true)
    → showPropertiesFor(target)
```

badge 点击后实质是"借 MonitorItem 的选中态来打开属性面板"，MonitorItem 本身不贡献额外功能。

### 影响范围

| 依赖方 | 用途 |
|--------|------|
| `TopologyMonitor` | `position`/`size` 字段仅用于 MonitorItem |
| `TopologyScene::addMonitorItem/findMonitorItem` | 创建/查找 MonitorItem |
| `TopologyScene::monitor_items_` 容器 | 持有 MonitorItem 指针 |
| `TopologyScene::clearScene` | 清空 monitor_items_ |
| `TopologyScene::loadFromDocument` | 遍历 monitors[] 调用 addMonitorItem |
| `TopologyScene::syncPositionsToDocument` | 回写 monitor_items_ 位置到 doc |
| `TopologyScene::mouseReleaseEvent` | 创建 MoveMonitorCommand |
| `TopologyScene::continueConnectionDrag` | MonitorPortItem 拖拽处理 |
| `TopologyScene.h` 前置声明 | `MonitorItem`/`MonitorPortItem` 前向声明 |
| `topology_items.h` 前置声明 | `MonitorPortItem` 前向声明 |
| `TopologyOutlineWidget` | 导航到 MonitorItem（onOutlineNavigate case 5） |
| `PropertyPanelWidget::showPropertiesFor` | `qgraphicsitem_cast<MonitorItem*>` 分支 |
| `ConnectionItem::mousePressEvent` badge 分支 | `findMonitorItem` → `setSelected` |
| `TopologyEditorWidget::addMonitorRequested` lambda | `findMonitorItem` → `centerOn`；`mon.position` 赋值 |
| `TopologyEditorWidget::onPaste` | `mon.position`/`mon.size` 赋值 + `findMonitorItem` → `centerOn` |
| `TopologyEditorWidget::onDeleteItem` | `qgraphicsitem_cast<MonitorItem*>` 分支 |
| `TopologyEditorWidget::onSelectionChanged` | 同步大纲树时 MonitorItem 分支 |
| `TopologyEditorWidget::rebuildSceneAndRestoreSelection` | MonitorItem 类型保存/恢复 |
| `TopologyEditorWidget::onCopy` | monitor position/size/sizeWidth/sizeHeight 字段写入 |
| `TopologyEditorWidget::doAlign` | MonitorItem 收集和 MoveMonitorCommand 创建 |
| `TopologyEditorWidget::doDistribute` | MonitorItem 收集和 MoveMonitorCommand 创建 |
| `TopologyEditorWidget::updateAlignDistributeActions` | MonitorItem 可移动性判断 |
| `MoveMonitorCommand` | 撤销监听器移动 |
| `ResizeItemCommand::Monitor` 分支 | 撤销监听器大小调整 |
| `TopologyJsonSerializer::serialize` | position/size 序列化写入 |
| `TopologyJsonSerializer::deserialize` | position/size 反序列化读取 |
| `topology_items.h/.cpp MonitorItem` | 类定义、paint、port、resize |
| `topology_items.h/.cpp MonitorPortItem` | 端口图元 |
| `TopologyBlockItem::onResizeFinished` override | 保存位置到 doc |

## 方案

### 核心改动

**监听器属性入口从 MonitorItem 选中切换到 badge 直接通知**。

### 新链路设计

```
点击 badge → ConnectionItem::mousePressEvent
  → QPointF 距离检测判定在 badge 范围内
  → qobject_cast<TopologyScene*>(scene())
    → scene->emitMonitorBadgeClicked(connIdx, monIdx)
      → TopologyScene 发射 monitorBadgeClicked(int connIdx, int monIdx) 信号
        → TopologyEditorWidget 接收
          → property_panel_->showMonitorProperties(monIdx)
          → view_->centerOn(对应 connectionItem)
          → scene_->clearSelection() + connectionItem->setSelected(true)

大纲树点击监听器 → onOutlineNavigate case 5
  → property_panel_->showMonitorProperties(mainIndex)
  → view_->centerOn(对应 connectionItem)
  → 选中对应 connectionItem

右键连线→添加监听器 创建后
  → 不再创建 MonitorItem，直接更新 badge
  → 选中该连线 + 在 property panel 显示监听器属性
```

> **注意 1**：`ConnectionItem` 继承自 `QGraphicsPathItem`（最终基类 `QGraphicsItem`），**不是 QObject**，无法声明 Qt 信号。因此 badge 点击后**调用场景的方法** `emitMonitorBadgeClicked(int, int)`，由 `TopologyScene`（是 QObject）发射 `monitorBadgeClicked(int connIdx, int monIdx)` 信号供 `TopologyEditorWidget` 连接。

> **注意 2**：存在时序冲突——`ConnectionItem::mousePressEvent` 执行 badge 处理后，`TopologyScene::mousePressEvent` 会继续执行 `selectedItems()` 并 `emit itemSelected(selected)`，触发 `showPropertiesFor(connectionItem)` 覆盖已设置的监听器属性面板。解决方案：在 `TopologyScene` 中新增 `bool badge_click_handled_` 标记，`emitMonitorBadgeClicked()` 中置 true，`mousePressEvent` 中检测到此标记为 true 时跳过 `itemSelected` 发射并重置标记。

### 新增

| 位置 | 新增内容 | 说明 |
|------|---------|------|
| `TopologyScene.h` | 信号 `monitorBadgeClicked(int connIdx, int monIdx)` | badge 点击通知 |
| `TopologyScene.cpp` | 方法 `emitMonitorBadgeClicked(int connIdx, int monIdx)` | 被 ConnectionItem 调用，发射信号 |
| `PropertyPanelWidget.h/.cpp` | 方法 `showMonitorProperties(int monitorIndex)` | 直接显示监听器属性页 |
| `TopologyEditorWidget.cpp` | 新增 slot `onMonitorBadgeClicked(int connIdx, int monIdx)` 实现体，放置在 `onOutlineNavigate` 附近：`clearSelection()` + 选中对应 ConnectionItem + `centerOn` + `property_panel_->showMonitorProperties(monIdx)` |
| `TopologyEditorWidget.cpp:initSignals()` | 新增 `connect(scene_, &TopologyScene::monitorBadgeClicked, this, &TopologyEditorWidget::onMonitorBadgeClicked)` |

### 清理清单

完整清理清单（按编译单元分组）：

#### 1. TopologyDocument — 数据模型精简

| 文件 | 改动 |
|------|------|
| `TopologyDocument.h:88-89` | `TopologyMonitor` 移除 `position`/`size` 字段 |

#### 2. TopologyJsonSerializer — 序列化精简

| 文件 | 改动 |
|------|------|
| `TopologyJsonSerializer.cpp:129-132` | serialize：移除 `positionX`/`positionY`/`size` 写入 |
| `TopologyJsonSerializer.cpp:290-296` | deserialize：读取旧字段值但忽略（QJsonObject 读取不存在的 key 返回默认值，不会出错，不做任何存储） |

#### 3. topology_items — MonitorItem/MonitorPortItem 类 + badge 改造

| 文件 | 改动 |
|------|------|
| `topology_items.h:17` | 移除 `class MonitorItem;` 前置声明 |
| `topology_items.h:230` | 移除 `class MonitorPortItem;` 前置声明（在 MonitorItem 类声明之后，会被下方的范围清理覆盖） |
| `topology_items.h:234-265` | 移除 `MonitorItem` 类声明 |
| `topology_items.h:269-289` | 移除 `MonitorPortItem` 类声明 |
| `topology_items.cpp:816-960` | 移除 `MonitorItem` 全部实现（构造/paintContent/layoutPort/颜色/onResizeFinished/calcContentHeight） |
| `topology_items.cpp:926-960` | 移除 `MonitorPortItem` 全部实现（构造/boundingRect/shape/paint/sceneCenter） |
| `topology_items.cpp:791-808` | `ConnectionItem::mousePressEvent` badge 分支：从 `findMonitorItem(idx)->setSelected(true)` 改为调用 `scene->emitMonitorBadgeClicked(conn_index_, monitor_index_)`，保持区域检测代码不变 |

#### 4. TopologyScene — 场景层清理

| 文件 | 改动 |
|------|------|
| `TopologyScene.h:17,19` | 移除 `MonitorItem`/`MonitorPortItem` 前置声明 |
| `TopologyScene.h:37,53,61,91` | 移除 `addMonitorItem()` 声明、`findMonitorItem()` 声明、`monitorPortItemAt()` 声明、`monitor_items_` 容器 |
| `TopologyScene.h` | 新增 `monitorBadgeClicked(int connIdx, int monIdx)` 信号声明 |
| `TopologyScene.h` | 新增 `void emitMonitorBadgeClicked(int connIdx, int monIdx)` 公共方法 |
| `TopologyScene.h` | 新增 `bool badge_click_handled_ = false;` 成员变量（时序冲突防护） |
| `TopologyScene.cpp:mousePressEvent` | 调用 `QGraphicsScene::mousePressEvent(event)` 后，检查 `badge_click_handled_`，为 true 时跳过 `itemSelected` 发射并重置 |
| `TopologyScene.cpp:38-41` | `loadFromDocument()` 移除 monitorItems 创建循环 |
| `TopologyScene.cpp:59-63` | `syncPositionsToDocument()` 移除 monitor 位置回写循环 |
| `TopologyScene.cpp:131-137` | 移除 `addMonitorItem()` 实现 |
| `TopologyScene.cpp:441-447` | 移除 `findMonitorItem()` 实现 |
| `TopologyScene.cpp:469` | `clearScene()` 移除 `monitor_items_.clear()` |
| `TopologyScene.cpp:515-518` | `mouseReleaseEvent()` 移除 `MoveMonitorCommand` 创建分支 |
| `TopologyScene.cpp:188` | `continueConnectionDrag()` 移除 `MonitorPortItem*` 拖拽处理分支 |

#### 5. PropertyPanelWidget — 属性面板改造

| 文件 | 改动 |
|------|------|
| `PropertyPanelWidget.cpp:272-308` | `showPropertiesFor()` 移除 `qgraphicsitem_cast<MonitorItem*>` 分支 |
| `PropertyPanelWidget.cpp` | 新增方法 `showMonitorProperties(int monitorIndex)`：复用被移除分支的逻辑（查找 doc_->monitor(index)，填充 name/connectionId/displayMode，设置 editing_monitor_index_，切换到 PageMonitor） |

#### 6. TopologyEditorWidget — 编辑器全面清理

| 文件 | 改动 |
|------|------|
| `TopologyEditorWidget.h` | 新增 `onMonitorBadgeClicked(int connIdx, int monIdx)` slot 声明 |
| `TopologyEditorWidget.cpp:596` | `addMonitorRequested` lambda：移除 `mon.position = ...` 赋值；移除 `findMonitorItem(cmd->monitorIndex())` → `centerOn`，改为选中对应 connectionItem + 显示监听器属性 |
| `TopologyEditorWidget.cpp:599-601` | （同上 lambda）新增 `scene_->clearSelection()` + `connItem->setSelected(true)` + `property_panel_->showMonitorProperties(cmd->monitorIndex())` |
| `TopologyEditorWidget.cpp:976` | `onDeleteItem`：移除 `qgraphicsitem_cast<MonitorItem*>` 分支 |
| `TopologyEditorWidget.cpp:1035` | `onSelectionChanged`：移除同步大纲树时 MonitorItem 分支 |
| `TopologyEditorWidget.cpp:1077-1128` | `rebuildSceneAndRestoreSelection`：移除 MonitorItem 类型(5)的保存/恢复 |
| `TopologyEditorWidget.cpp:1233` | `onCopy`：monitor 序列化移除 `positionX`/`positionY`/`sizeWidth`/`sizeHeight` 字段，新增写入 `connectionId`/`displayMode` |
| `TopologyEditorWidget.cpp:1367-1370` | `onPaste`：移除 `mon.position = ...` 和 `mon.size = ...` 赋值；确保读取 `connectionId`/`displayMode` 字段 |
| `TopologyEditorWidget.cpp:1573` | `updateAlignDistributeActions`：移除 MonitorItem 可移动性判断 |
| `TopologyEditorWidget.cpp:1603-1604` | `doAlign`：移除 MonitorItem 收集 |
| `TopologyEditorWidget.cpp:1671-1672` | `doAlign`：移除 `MoveMonitorCommand` 创建 |
| `TopologyEditorWidget.cpp:1701-1702` | `doDistribute`：移除 MonitorItem 收集 |
| `TopologyEditorWidget.cpp:1764-1765` | `doDistribute`：移除 `MoveMonitorCommand` 创建 |
| `TopologyEditorWidget.cpp:1422` | `onOutlineNavigate` case 5：移除 `target = scene_->findMonitorItem(mainIndex)`，改为 `property_panel_->showMonitorProperties(mainIndex)` + 选中对应 connectionItem 并 centerOn |

#### 7. UndoCommands — 撤销命令清理

| 文件 | 改动 |
|------|------|
| `UndoCommands.h:171-185` | 移除 `MoveMonitorCommand` 类声明 |
| `UndoCommands.cpp:282-303` | 移除 `MoveMonitorCommand` 全部实现（构造/undo/redo） |
| `UndoCommands.cpp:439-466` | `ResizeItemCommand::undo/redo` 移除 `case Monitor:` 分支 |

#### 8. 前向声明清理（与上述条目重叠，此处汇总）

| 文件 | 改动 |
|------|------|
| `topology_items.h:17` | 移除 `class MonitorItem;`（前向声明） |
| `TopologyScene.h:17` | 移除 `class MonitorPortItem;`（前向声明） |
| `TopologyScene.h:19` | 移除 `class MonitorItem;`（前向声明） |

### 不清理的内容

| 代码 | 保留理由 |
|------|----------|
| `TopologyMonitor` 结构体自身 | `name`/`connectionId`/`displayMode` 仍为必需 |
| `AddMonitorCommand` / `RemoveMonitorCommand` | 监听器创建和删除仍需要 |
| `monitor_items_` 之外的其他 items 容器 | UutItem/DeviceItem/ConnectionItem 仍需要 |
| `TopologyOutlineWidget::addMonitorItem` | 大纲树显示监听器节点仍需要 |
| `updateMonitorBadges()` | 连线 badge 更新逻辑不变 |
| `editing_monitor_index_` | 属性面板仍需要记录当前编辑的监听器索引 |

### 向前兼容

旧 `.etopo` 文件中 `positionX`/`positionY`/`size` 字段在 `deserialize` 中会被 `QJsonObject::toDouble()`/`toArray()` 安全读取但不做任何存储。由于 QJsonObject 访问不存在的 key 返回默认值（0 / 空数组），所以即使移除这些读取行也不会报错。推荐直接删除读取行，让它们被自然忽略。

### 影响评估

- **场景**：不再显示监听器方块和端口，连线 badge 是唯一的监听器可视化标记
- **属性面板**：点击 badge 或大纲树节点时直接显示监听器属性，流程缩短一步
- **数据模型**：TopologyMonitor 更轻量（6 字段 → 3 字段）
- **撤销**：不再需要 MoveMonitorCommand，ResizeItemCommand 移除 Monitor 分支
- **序列化**：不再写入 position/size，减小 .etopo 文件体积
- **场景交互**：点击连线 badge → 选中该连线 + 弹出监听器属性面板，视觉反馈更直观
