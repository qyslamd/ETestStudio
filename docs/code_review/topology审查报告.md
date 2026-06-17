# src/topology 完整代码审查报告

## Context

本次审查范围是 `D:\trae_workspace\etest-demo\src\topology` 下的拓扑编辑器模块。该模块已通过 `examples/topology-demo/CMakeLists.txt` 独立构建 demo，便于复现与验证问题。

审查目标不是立即修改代码，而是先完整识别真实问题，按严重级别给出定位、影响和修复建议。审查重点包括：

- 文档模型、序列化、撤销栈、属性面板、场景/视图交互、导出与清理逻辑。
- Qt 生命周期、QGraphicsItem ownership、信号槽、QUndoCommand 幂等性。
- 字符串引用带来的重命名同步风险。
- 项目规则：禁止在 C++ 中硬编码 `setStyleSheet`，Qt UI 初始化与信号连接分离，Google C++ 风格。

审查覆盖的主要文件：

- `TopologyDocument.*`
- `TopologyJsonSerializer.*`
- `topology_items.*`
- `TopologyScene.*`
- `TopologyView.*`
- `TopologyEditorWidget.*`
- `UndoCommands.*`
- `PropertyPanelWidget.*`
- `DevicePaletteWidget.*`
- `TopologyOutlineWidget.*`
- `TopologySceneRenderer.*`
- `ConnectionCleanup.*`
- `DeviceTemplateManager.*`
- `TopologyPathRouter.*`
- `TopologyTheme.*`
- `ComboBoxDelegate.*`
- `CMakeLists.txt`

## 总体结论

模块功能已经较完整，但目前有几类风险需要优先处理：

1. **撤销/重做存在系统性数据破坏风险**：多个 `Remove*Command` 的 undo 使用 append 恢复，redo 仍按旧索引删除，删除非末尾元素后 undo/redo 会删错对象。
2. **重命名同步不完整**：UUT/设备/端口重命名只同步了部分 `TopologyConnection` 字段，漏掉 monitor tap 和设备端口等引用，会导致连线/监听器挂载丢失。
3. **属性面板端口表全量替换会丢字段**：只保留 name/direction/functionType，丢失 allowedDeviceTypes、positionHint、portStyle。
4. **右键样式修改绕过撤销栈和修改状态**：端口样式、连线样式直接改文档和 item，`isModified()` 不可靠。
5. **清理无效挂载的批量删除顺序有风险**：同一 monitor 多个 invalid tap 时，按索引删除可能删错。
6. **TopologyEditorWidget / PropertyPanelWidget 明显过大**：分别约 1789 行和 1168 行，后续维护成本高，建议逐步拆分。

## 严重问题

### S1. Remove* / UnTap* 命令 undo 追加、redo 用旧索引，可能删除错误对象

位置：

- `src/topology/UndoCommands.cpp:49`
- `src/topology/UndoCommands.cpp:61`
- `src/topology/UndoCommands.cpp:114`
- `src/topology/UndoCommands.cpp:126`
- `src/topology/UndoCommands.cpp:185`
- `src/topology/UndoCommands.cpp:193`
- `src/topology/UndoCommands.cpp:413`
- `src/topology/UndoCommands.cpp:417`
- `src/topology/UndoCommands.cpp:437`
- `src/topology/UndoCommands.cpp:441`
- `src/topology/UndoCommands.cpp:498`
- `src/topology/UndoCommands.cpp:510`
- `src/topology/UndoCommands.cpp:571`
- `src/topology/UndoCommands.cpp:575`

现象：

- `RemoveProductCommand::redo()` 按构造时保存的 `index_` 删除。
- `undo()` 用 `doc_->addProduct(product_)` 追加恢复到末尾。
- 再次 redo 仍按旧 `index_` 删除，此时该索引已经可能是另一个对象。

复现示例：

1. products = `[A, B]`。
2. 删除 A：redo 删除 index 0，剩 `[B]`。
3. undo：append A，变 `[B, A]`。
4. redo：继续删除 index 0，结果删除 B，A 还在。

影响：

- 删除非末尾 UUT/设备/连线/端口/tap 后执行 undo → redo，会破坏文档数据。
- 场景重建会忠实反映错误数据，用户看到的是错误拓扑。

建议修复：

- 推荐在 `TopologyDocument` 增加按索引插入 API：
  - `insertProduct(int index, const TopologyProduct&)`
  - `insertDevice(int index, const TopologyDevice&)`
  - `insertConnection(int index, const TopologyConnection&)`
  - `insertProductPort(int productIndex, int portIndex, const TopologyPort&)`
  - `insertDevicePort(int deviceIndex, int portIndex, const TopologyDevicePort&)`
  - `insertTap(int monitorIndex, int tapIndex, const TopologyMonitorTap&)`
- 所有 Remove/UnTap 命令的 undo 改为恢复原索引。
- Add*Command 仍可保持 append + 记录 index 的模式，不必一起改。

验证：

- 增加撤销栈单元测试：删除第一个/中间元素，undo，redo，确认删除的仍是原对象。
- 覆盖 UUT、设备、连线、UUT 端口、设备端口、monitor tap。

### S2. 重命名同步不完整，导致连线/监听器引用丢失

位置：

- `src/topology/PropertyPanelWidget.cpp:625`
- `src/topology/PropertyPanelWidget.cpp:830`
- `src/topology/PropertyPanelWidget.cpp:927`
- `src/topology/PropertyPanelWidget.cpp:1097`

现状：

- `onUutNameChanged()` 只同步 `TopologyConnection::productName`，漏掉 `TopologyMonitorTap::productName`。
- `onPortNameChanged()` 只改 UUT 端口名，漏掉：
  - `TopologyConnection::portName`
  - `TopologyMonitorTap::portName`
- `onDeviceNameChanged()` 只改设备名，漏掉：
  - `TopologyConnection::deviceName`
  - `TopologyMonitorTap::deviceName`
- `onDevicePortNameChanged()` 只改设备端口名，漏掉：
  - `TopologyConnection::devicePort`
  - `TopologyMonitorTap::devicePort`

影响：

- 用户修改名称后，下一次场景重建时连线/监听器按旧名字查找失败。
- 保存后旧引用被写入 `.etopo`，重开项目后连线或监听器挂载永久丢失。
- `ConnectionCleanup` 会把这些项识别为无效，用户清理后数据被删除。

建议修复：

- 不建议继续在 UI lambda 中散落同步逻辑。
- 在 `TopologyDocument` 增加明确的 rename API：
  - `renameProduct(int index, const QString& newName)`
  - `renameProductPort(int productIndex, int portIndex, const QString& newName)`
  - `renameDevice(int index, const QString& newName)`
  - `renameDevicePort(int deviceIndex, int portIndex, const QString& newName)`
- rename API 内部统一同步 `connections_` 和 `monitors_[].taps`。
- `PropertyCommand` 的 undo/redo 调用这些 API，而不是直接改字段。

验证：

- 建 UUT-设备连线和监听器挂载。
- 分别改 UUT 名、UUT 端口名、设备名、设备端口名。
- undo/redo 后确认连线和 tap 均存在且字段同步。
- 保存/重开 `.etopo` 后确认引用仍有效。

### S3. 清理无效监听器挂载时可能按错误索引删除

位置：

- `src/topology/ConnectionCleanup.cpp:59`
- `src/topology/TopologyEditorWidget.cpp:1504`
- `src/topology/TopologyEditorWidget.cpp:1511`

问题：

- `ConnectionCleanup::findInvalid()` 对同一个 monitor 的 taps 按降序收集。
- `onCleanupInvalidConnections()` 又从 `invalid.size() - 1` 反向创建 `UnTapConnectionCommand`。
- 两次反向后，同一 monitor 的 tap 可能按升序执行删除。
- 按索引删除时，前一个删除会导致后续索引前移，可能删错 tap。

影响：

- 同一监听器存在多个无效 tap 时，清理可能删除有效 tap 或漏删无效 tap。

建议修复：

- 对 invalid entries 明确排序：
  - Connection 按 `index` 降序。
  - MonitorTap 按 `(monIdx, index)` 降序。
- 或者将 `InvalidEntry` 设计成携带 tap 的完整四元组，删除时按内容匹配而不是按易失索引。

验证：

- 一个 monitor 下创建多个 taps，其中多个无效、多个有效。
- 执行清理，确认只删除无效项。

## 重要问题

### I1. UUT 端口表 apply 会丢 allowedDeviceTypes / positionHint / portStyle

位置：

- `src/topology/PropertyPanelWidget.cpp:768`
- `src/topology/PropertyPanelWidget.cpp:780`
- `src/topology/PropertyPanelWidget.cpp:821`

问题：

`applyUutPorts()` 从表格重建 `TopologyPort` 时只写：

- `name`
- `direction`
- `functionType`

未保留：

- `allowedDeviceTypes`
- `positionHint`
- `portStyle`

影响：

- 只要 UUT 端口表 dirty，离开页面后整体替换端口数组。
- 用户可能只改一个端口名，却把所有端口的连接规则、位置提示、样式清空。

建议修复：

- 按行号从 `saved_uut_ports_` 继承未展示字段。
- 后续如支持端口排序，再改用稳定 ID 或旧 name 映射。

### I2. 设备端口表 apply 会丢 positionHint / portStyle

位置：

- `src/topology/PropertyPanelWidget.cpp:1020`
- `src/topology/PropertyPanelWidget.cpp:1053`

问题同 I1，设备端口重建时未保留：

- `positionHint`
- `portStyle`

建议修复：

- 按行从 `saved_device_ports_` 继承未编辑字段。

### I3. 端口样式和连线样式修改绕过撤销栈与 modified 状态

位置：

- `src/topology/TopologyView.cpp:213`
- `src/topology/TopologyView.cpp:233`
- `src/topology/TopologyView.cpp:278`
- `src/topology/topology_items.cpp:48`
- `src/topology/topology_items.cpp:486`
- `src/topology/topology_items.cpp:802`

问题：

- 右键菜单直接调用 `setPortStyle()` / `setStyle()`。
- item 方法直接写文档字段。
- 不走 `QUndoCommand`，不触发 cleanChanged，不更新 `isModified()`。

影响：

- 样式变化不可撤销。
- 用户看到未修改状态，但保存内容已经改变。
- 关闭编辑器时可能不提示保存。

建议修复：

- 增加 `SetProductPortStyleCommand`、`SetDevicePortStyleCommand`、`SetConnectionStyleCommand`。
- `TopologyView` 不直接改 item，改为发 signal，由 `TopologyEditorWidget` push command。

### I4. 连接拖拽完成时源端口索引未校验，可能越界

位置：

- `src/topology/TopologyScene.cpp:317`
- `src/topology/TopologyScene.cpp:318`
- `src/topology/TopologyScene.cpp:331`
- `src/topology/TopologyScene.cpp:332`

问题：

- UUT→设备路径只校验目标设备端口索引，未校验 `srcPort->portIndex()`。
- 设备→UUT路径只校验目标 UUT 端口索引，未校验 `srcDevPort->portIndex()`。

建议修复：

- 两条路径均同时校验源端口和目标端口索引：
  - `srcPort->portIndex() >= 0 && srcPort->portIndex() < prod->ports.size()`
  - `srcDevPort->portIndex() >= 0 && srcDevPort->portIndex() < dev->ports.size()`

### I5. 删除连线重新按四元组匹配，可能删错重复连线

位置：

- `src/topology/TopologyEditorWidget.cpp:956`
- `src/topology/TopologyEditorWidget.cpp:975`

问题：

- `ConnectionItem` 已有 `connectionIndex()`。
- 删除时却重新通过 product/port/device/devicePort 四元组遍历匹配。
- 重复连线或同名实体存在时可能删错。

建议修复：

- 直接使用 `conn->connectionIndex()` 创建 `RemoveConnectionCommand`。
- 同时对 index 做边界校验。

### I6. `RemoveProductPortCommand::redo()` 可能空指针解引用

位置：

- `src/topology/UndoCommands.cpp:510`
- `src/topology/UndoCommands.cpp:515`

问题：

- `doc_->product(product_index_)->name` 未判空。

建议修复：

- 先取 `auto* prod = doc_->product(product_index_)`。
- 若为空，直接返回或跳过级联删除。

### I7. 级联恢复连线时丢失 `PathStyle`

位置：

- `src/topology/UndoCommands.h:41`
- `src/topology/UndoCommands.cpp:51`
- `src/topology/UndoCommands.cpp:117`
- `src/topology/UndoCommands.cpp:501`

问题：

- RemoveProduct/RemoveDevice/RemoveProductPort 保存级联连线时只保存四个名称字段。
- 恢复时 `TopologyConnection::style` 使用默认值。

影响：

- 用户设置过连线样式后，删除实体再 undo，连线样式丢失。

建议修复：

- `ConnEntry` 增加 `PathStyle style`。
- 保存时记录 `c->style`，恢复时赋回。

### I8. `TopologySceneRenderer` SVG/PDF 导出无条件返回 true

位置：

- `src/topology/TopologySceneRenderer.cpp:45`
- `src/topology/TopologySceneRenderer.cpp:61`

问题：

- PNG 使用 `image.save()` 返回值。
- SVG/PDF 创建 painter 后无论文件是否写入成功都返回 true。

影响：

- 权限不足、路径非法、磁盘满时 UI 仍提示导出成功。

建议修复：

- 使用 `QPainter painter; if (!painter.begin(&gen)) return false;`。
- PDF 同理检查 `painter.begin(&writer)`。
- 导出后可额外检查文件存在且大小大于 0。

### I9. `setStyleSheet` 硬编码违反项目规则

位置：

- `src/topology/TopologyEditorWidget.cpp:250`
- `src/topology/TopologyEditorWidget.cpp:258`

问题：

- loading overlay 直接在 C++ 中拼接样式。
- 项目规则要求 Qt 样式写入 `src/app/resources/styles/` 下 QSS 文件，通过 `setObjectName` 定位。

建议修复：

- overlay 和 label 设置 objectName。
- 样式迁移到 QSS。

### I10. 粘贴 toolbar action 永久禁用

位置：

- `src/topology/TopologyEditorWidget.cpp:318`

问题：

- `paste_action_` 初始化为 disabled 后，后续没有重新启用。
- Ctrl+V 快捷键可用，但工具栏按钮一直灰色。

建议修复：

- `onCopy()` 成功后启用。
- 或监听剪贴板变化同步 action 状态。

## 一般问题

### G1. `TopologyDocument` 无名称唯一性约束

位置：

- `src/topology/TopologyDocument.cpp:91`
- `src/topology/TopologyDocument.cpp:118`
- `src/topology/TopologyDocument.cpp:331`

影响：

- 连线/监听器引用靠名称，重名会导致绑定歧义。
- 删除一个同名实体后连线可能跳绑到另一个同名实体。

建议：

- add/rename 时校验同类对象名称唯一。
- 或更长期：引入稳定 ID，名称仅作为显示字段。

### G2. 文档层删除实体不做级联清理

位置：

- `src/topology/TopologyDocument.cpp:98`
- `src/topology/TopologyDocument.cpp:125`
- `src/topology/TopologyDocument.cpp:338`

问题：

- 级联清理逻辑在 UndoCommands 中。
- 直接调用文档 API 删除实体会留下悬空连线/tap。

建议：

- 将级联规则下沉到 `TopologyDocument`，命令只负责调用文档服务和保存 undo 数据。

### G3. JSON 反序列化校验不足

位置：

- `src/topology/TopologyJsonSerializer.cpp:141`

问题：

- 缺字段/类型错误时大量使用 Qt 默认值静默兜底。
- `deserialize()` 基本总是返回 true。

建议：

- 校验 version、根对象、关键数组字段。
- 关键字段缺失时设置 `last_error_` 并返回 false。

### G4. DeviceTemplateManager 保存模板丢字段、写入未校验

位置：

- `src/topology/DeviceTemplateManager.cpp:34`
- `src/topology/DeviceTemplateManager.cpp:50`

问题：

- 设备端口模板只保存 `name/direction/functionType`。
- 漏掉 `positionHint` / `portStyle`。
- `file.write()` 返回值未检查。

建议：

- 模板端口补齐 `positionHint` / `portStyle`。
- 检查 write/flush 结果。

### G5. `TopologyOutlineWidget::applyFilter()` 清空过滤时未递归恢复孙子节点

位置：

- `src/topology/TopologyOutlineWidget.cpp:221`

影响：

- 输入过滤词隐藏端口后，清空搜索框可能只恢复父/子节点，端口等孙子节点仍隐藏。

建议：

- 清空过滤时也递归调用子节点。

### G6. `PropertyPanelWidget::showPropertiesFor()` 离开设备页时吞掉首次点击

位置：

- `src/topology/PropertyPanelWidget.cpp:48`
- `src/topology/PropertyPanelWidget.cpp:65`

问题：

- 离开设备页时保存编辑后直接 return，不继续显示新选中项。
- UUT 页没有此 return，行为不一致。

建议：

- 去掉 return，保存后继续 fall-through 到新 item 页面。

### G7. UUT 宽高 SpinBox 每 tick 都生成独立撤销命令并重复刷新

位置：

- `src/topology/PropertyPanelWidget.cpp:664`
- `src/topology/PropertyPanelWidget.cpp:686`
- `src/topology/PropertyPanelWidget.cpp:683`
- `src/topology/PropertyPanelWidget.cpp:705`

问题：

- `valueChanged` 每一格都 push 一条 `PropertyCommand`。
- push 后又 `emit documentChanged()`，与 undoStack indexChanged 造成重复重建。

建议：

- 尺寸命令支持 `mergeWith()`。
- 移除多余 `emit documentChanged()`，统一由 undoStack 驱动刷新。

### G8. 复制/粘贴不处理选中元素之间的连线

位置：

- `src/topology/TopologyEditorWidget.cpp:1153`
- `src/topology/TopologyEditorWidget.cpp:1251`

影响：

- 复制一组互连 UUT/设备后粘贴，元素存在但连线丢失。

建议：

- 复制时记录选中元素内部的连接四元组。
- 粘贴时建立旧名到新名映射后恢复连线。

### G9. 粘贴多个元素没有宏命令，撤销需要多次

位置：

- `src/topology/TopologyEditorWidget.cpp:1251`

建议：

- 用父 `QUndoCommand("粘贴")` 包装所有子命令。

### G10. 导出无后缀时恒补 `.png`，忽略用户选择的过滤器

位置：

- `src/topology/TopologyEditorWidget.cpp:1446`

影响：

- 用户选择 SVG/PDF 过滤器但文件名无后缀时，边界情况下可能保存成 png。

建议：

- 使用 `QFileDialog::getSaveFileName()` 的 selectedFilter 输出参数，根据过滤器补后缀。

### G11. 主题切换后 TopologyView 背景和图例不刷新

位置：

- `src/topology/TopologyView.cpp:32`
- `src/topology/TopologyView.cpp:58`

建议：

- 订阅 ThemeManager 的主题变化，刷新背景 brush 和 legend cache。

### G12. MIME 常量重复定义

位置：

- `src/topology/TopologyScene.cpp:21`
- `src/topology/DevicePaletteWidget.cpp:15`

建议：

- 新建公共头或放入已有头，统一声明 `kTopologyDeviceMime`。

### G13. 拖动时路径和 tap 视觉全量刷新，规模大时会卡顿

位置：

- `src/topology/TopologyScene.cpp:449`
- `src/topology/topology_items.cpp:691`
- `src/topology/TopologyScene.cpp:210`

问题：

- item 移动时更新所有连线。
- 每条连线又遍历所有 scene items 计算障碍物。
- tap 虚线每次全删全建，并对路径做采样。

建议：

- 第一阶段先不优化，等真实拓扑规模扩大再处理。
- 后续可维护 endpoint→connection 索引和障碍物缓存。

### G14. `ConnectionItem` 持有裸端口 item 指针，依赖全量重建兜底

位置：

- `src/topology/topology_items.h:218`
- `src/topology/topology_items.cpp:686`

风险：

- 结构变更后如果局部删除端口 item，而 connection item 未立即销毁，会出现悬垂指针。
- 当前大量全量重建掩盖了风险。

建议：

- 短期：避免端口局部 clear/layout，统一全量重建。
- 长期：ConnectionItem 用稳定索引/ID 查找端点，或持弱引用并判空。

## 建议重构项

### R1. 拆分 `TopologyEditorWidget`

位置：

- `src/topology/TopologyEditorWidget.cpp`

建议逐步拆分：

- `TopologyClipboard`：copy/paste 序列化、唯一名生成、内部连线恢复。
- `TopologyFileController`：load/save/saveAs/异步加载/loading overlay。
- `TopologyExportController`：导出文件选择和调用 `TopologySceneRenderer`。
- `TopologyCleanupDialog`：无效项列表对话框和批量命令构建。
- `TopologyAlignController`：对齐/分布逻辑。

### R2. 拆分 `PropertyPanelWidget`

位置：

- `src/topology/PropertyPanelWidget.cpp`

建议按页面拆：

- `UutPropertyPage`
- `DevicePropertyPage`
- `PortPropertyPage`
- `DevicePortPropertyPage`
- `ConnectionPropertyPage`
- `MonitorPropertyPage`

面板主类只负责 page 切换、commit 当前页、load 新选中项。

## 推荐修复顺序

### 第一批：数据正确性（最高优先级）

1. 修 S1：Remove/UnTap 命令恢复原索引。
2. 修 S2：rename API 同步 connections/taps。
3. 修 I1/I2：端口表 apply 保留未展示字段。
4. 修 S3：无效 tap 清理删除顺序。
5. 修 I4/I5/I6：越界/删错/空指针防御。

### 第二批：撤销栈和 modified 状态一致性

1. 修 I3：端口/连线样式走 QUndoCommand。
2. 修 I7：级联恢复保留 PathStyle。
3. 修 G7：SpinBox 命令合并、去掉重复 documentChanged。
4. 修 G9：粘贴改为宏命令。

### 第三批：项目规则与用户可见问题

1. 修 I9：移除 C++ setStyleSheet。
2. 修 I10：粘贴按钮状态同步。
3. 修 G5/G6/G10/G11：大纲过滤、属性页首次点击、导出后缀、主题刷新。

### 第四批：结构优化与性能

1. 拆分 TopologyEditorWidget。
2. 拆分 PropertyPanelWidget。
3. 优化拖动时连线/tap 全量刷新。
4. 长期引入稳定 ID，替代名称引用。

## 验证建议

### 单元测试

新增或扩展 topology 测试：

- `undo_commands_test.cpp`
  - 删除非末尾 UUT/设备/端口/连线/tap 后 undo/redo。
  - 级联删除后 undo，连线 style 保持。
- `property_panel_sync_test.cpp`
  - rename UUT/端口/设备/设备端口后 connections/taps 同步。
  - apply UUT/设备端口表后隐藏字段不丢。
- `connection_cleanup_test.cpp`
  - 同一 monitor 多个 invalid taps 混合有效 taps 时只删无效。
- `scene_renderer_test.cpp`
  - SVG/PDF 写失败路径返回 false。

### Demo 手工验证

使用 `examples/topology-demo`：

1. 打开默认或测试 `.etopo`。
2. 建 UUT/设备/监听器挂载。
3. 修改 UUT 名、端口名、设备名、设备端口名。
4. 执行 undo/redo、保存/重开。
5. 删除第一个 UUT/设备/端口，undo/redo，确认对象和连线正确。
6. 清理多个无效监听器挂载，确认有效挂载不被误删。
7. 修改端口/连线样式，确认 modified 状态、undo/redo、生效持久化。

## 当前建议

不要一次性修完所有问题。建议先做第一批数据正确性修复，并配套单元测试。第一批完成后再提交，再处理撤销栈/样式 modified 状态和 UI 规则问题。

# 渐进式修复计划

## 修复原则

1. **按风险分批修复**：先处理会造成数据损坏的问题，再处理撤销栈一致性、用户可见问题、规则问题，最后做结构拆分。
2. **每批独立验证和提交**：每个阶段完成后都要编译、运行相关单元测试，并用 `topology-demo` 做必要手工验证。
3. **先补测试再修实现**：撤销命令、文档引用同步、连接清理等逻辑相对独立，优先用单元测试固定回归场景。
4. **避免一次性大重构**：`TopologyEditorWidget` 和 `PropertyPanelWidget` 的拆分放到数据正确性稳定之后。

## Phase 0：建立回归测试基线

### 目标

先补最小回归测试，暴露当前已知问题，避免后续修复靠手工验证。

### 修改范围

- `tests/topology/CMakeLists.txt`
- 新增 `tests/topology/undo_commands_test.cpp`
- 新增 `tests/topology/property_sync_test.cpp`（或按实际需要拆成多个测试文件）

### 测试内容

1. 删除非末尾 UUT 后执行 undo/redo，确认 redo 删除的仍是原 UUT。
2. 删除非末尾设备后执行 undo/redo，确认 redo 删除的仍是原设备。
3. 删除非末尾连线后执行 undo/redo，确认 redo 删除的仍是原连线。
4. 删除非末尾 UUT 端口后执行 undo/redo，确认 redo 删除的仍是原端口。
5. 删除 monitor tap 后执行 undo/redo，确认 redo 删除的仍是原 tap。

### 验证

```bash
scripts/build_ninja.bat
cd build/ninja-debug
ctest -R topology --output-on-failure
```

### 提交建议

```txt
test(topology): 增加撤销命令回归测试
```

## Phase 1：修复 Remove/UnTap undo-redo 索引错误

### 目标

修复最高风险问题：删除非末尾元素后 undo/redo 会删错对象。

### 修改范围

- `src/topology/TopologyDocument.h`
- `src/topology/TopologyDocument.cpp`
- `src/topology/UndoCommands.h`
- `src/topology/UndoCommands.cpp`
- `tests/topology/undo_commands_test.cpp`

### 设计

在 `TopologyDocument` 中增加按索引插入 API：

```cpp
int insertProduct(int index, const TopologyProduct& product);
int insertDevice(int index, const TopologyDevice& device);
int insertConnection(int index, const TopologyConnection& connection);
int insertProductPort(int productIndex, int portIndex, const TopologyPort& port);
int insertDevicePort(int deviceIndex, int portIndex, const TopologyDevicePort& port);
int insertTap(int monitorIndex, int tapIndex, const TopologyMonitorTap& tap);
```

将以下命令的 `undo()` 从 append 改成 insert 原位置：

- `RemoveProductCommand`
- `RemoveDeviceCommand`
- `RemoveConnectionCommand`
- `RemoveMonitorCommand`
- `RemoveProductPortCommand`
- `RemoveDevicePortCommand`
- `UnTapConnectionCommand`

### 注意点

- `index < 0` 或 parent index 无效时返回 `-1`。
- `index > size` 建议返回 `-1`，不要静默 append，避免掩盖逻辑错误。
- `Add*Command` 暂不改，它们的 append + 记录 index 模式可以保留。

### 验证

- Phase 0 中 undo/redo 测试全部通过。
- `topology-demo` 中删除第一个 UUT/设备/端口，undo/redo 后对象正确。

### 提交建议

```txt
fix(topology): 修复删除命令撤销重做索引错乱
```

## Phase 2：修复重命名引用同步

### 目标

修复 UUT、设备、端口、设备端口改名后连线和监听器挂载丢失的问题。

### 修改范围

- `src/topology/TopologyDocument.h`
- `src/topology/TopologyDocument.cpp`
- `src/topology/PropertyPanelWidget.cpp`
- `tests/topology/property_sync_test.cpp`

### 设计

将引用同步逻辑下沉到 `TopologyDocument`，新增 rename API：

```cpp
bool renameProduct(int index, const QString& newName);
bool renameProductPort(int productIndex, int portIndex, const QString& newName);
bool renameDevice(int index, const QString& newName);
bool renameDevicePort(int deviceIndex, int portIndex, const QString& newName);
```

同步规则：

| 改名对象 | 同步字段 |
|----------|----------|
| UUT 名称 | `TopologyConnection::productName`、`TopologyMonitorTap::productName` |
| UUT 端口名 | `TopologyConnection::portName`、`TopologyMonitorTap::portName` |
| 设备名称 | `TopologyConnection::deviceName`、`TopologyMonitorTap::deviceName` |
| 设备端口名 | `TopologyConnection::devicePort`、`TopologyMonitorTap::devicePort` |

`PropertyPanelWidget` 中的 `PropertyCommand` undo/redo 调用这些 API，不再直接改字段。

### 注意点

- 改名前后名称相同则不入栈。
- 本阶段暂不做名称唯一性约束，避免扩大改动范围。

### 验证

1. 创建 UUT、设备、连线、monitor tap。
2. 分别修改 UUT 名、UUT 端口名、设备名、设备端口名。
3. undo/redo 后确认 connection 和 tap 字段均同步。
4. 保存/重开 `.etopo` 后确认引用仍有效。

### 提交建议

```txt
fix(topology): 修复拓扑元素改名后引用不同步
```

## Phase 3：修复端口表 apply 丢隐藏字段

### 目标

修复属性面板编辑端口表后，隐藏字段被清空的问题。

### 修改范围

- `src/topology/PropertyPanelWidget.cpp`
- `tests/topology/property_sync_test.cpp`

### 修复点

`applyUutPorts()` 重建 `TopologyPort` 时，从 `saved_uut_ports_` 继承：

```cpp
if (r < saved_uut_ports_.size()) {
  port.allowedDeviceTypes = saved_uut_ports_[r].allowedDeviceTypes;
  port.positionHint = saved_uut_ports_[r].positionHint;
  port.portStyle = saved_uut_ports_[r].portStyle;
}
```

`applyDevicePorts()` 重建 `TopologyDevicePort` 时，从 `saved_device_ports_` 继承：

```cpp
if (r < saved_device_ports_.size()) {
  port.positionHint = saved_device_ports_[r].positionHint;
  port.portStyle = saved_device_ports_[r].portStyle;
}
```

### 注意点

- 当前端口表不支持拖拽排序，按行继承可接受。
- 以后若支持排序，再改成稳定 ID 或旧名称映射。

### 验证

- UUT 端口修改 name/direction/functionType 后，`allowedDeviceTypes`、`positionHint`、`portStyle` 不丢。
- 设备端口修改 name/direction/functionType 后，`positionHint`、`portStyle` 不丢。

### 提交建议

```txt
fix(topology): 保留端口表编辑中的隐藏字段
```

## Phase 4：修复无效监听器挂载清理顺序

### 目标

修复同一 monitor 多个无效 tap 清理时可能删错的问题。

### 修改范围

- `src/topology/TopologyEditorWidget.cpp`
- `tests/topology/connection_cleanup_test.cpp`

### 方案

在 `TopologyEditorWidget::onCleanupInvalidConnections()` 构建批量命令前排序 invalid entries：

1. `Connection` 按 `index` 降序。
2. `MonitorTap` 按 `(monIdx, index)` 降序。

排序后按正向创建子命令，不再对整个列表做二次反转。

### 验证

- 一个 monitor 下有 5 个 taps。
- tap 1、3、4 无效，tap 0、2 有效。
- 执行清理后只删除 1、3、4，0、2 保留。

### 提交建议

```txt
fix(topology): 修复无效监听器挂载批量清理顺序
```

## Phase 5：修复边界防御问题

### 目标

集中处理几个小但真实的正确性问题。

### 修改范围

- `src/topology/TopologyScene.cpp`
- `src/topology/TopologyEditorWidget.cpp`
- `src/topology/UndoCommands.cpp`

### 修复项

1. `TopologyScene::finishConnectionDrag()` 同时校验源端口和目标端口索引。
2. `TopologyEditorWidget::onDeleteItem()` 删除连线时直接使用 `ConnectionItem::connectionIndex()`。
3. `RemoveProductPortCommand::redo()` 先判空 `doc_->product(product_index_)`。

### 提交建议

```txt
fix(topology): 增强连线和端口删除边界检查
```

## Phase 6：修复样式修改的撤销栈和 modified 状态

### 目标

让端口样式、连线样式走 `QUndoCommand`，保证可撤销、可重做、`isModified()` 正确。

### 修改范围

- `src/topology/UndoCommands.h`
- `src/topology/UndoCommands.cpp`
- `src/topology/TopologyView.h`
- `src/topology/TopologyView.cpp`
- `src/topology/TopologyEditorWidget.cpp`
- `src/topology/topology_items.cpp`

### 设计

新增命令：

```cpp
class SetProductPortStyleCommand;
class SetDevicePortStyleCommand;
class SetConnectionStyleCommand;
```

`TopologyView` 右键菜单不再直接调用 `setPortStyle()` / `setStyle()`，改为发信号：

```cpp
void productPortStyleChangeRequested(int productIndex, int portIndex, PortStyle style);
void devicePortStyleChangeRequested(int deviceIndex, int portIndex, PortStyle style);
void connectionStyleChangeRequested(int connectionIndex, PathStyle style);
```

由 `TopologyEditorWidget` 接收信号并 push command。

### 注意点

- `topology_items.cpp` 中现有 `setPortStyle()` / `setStyle()` 建议改成只做视觉刷新，或重命名为 `setVisualPortStyle()` / `setVisualStyle()`。
- 避免后续继续误用 item 方法直接写文档。

### 提交建议

```txt
fix(topology): 让端口和连线样式支持撤销重做
```

## Phase 7：修复级联恢复丢失连线样式

### 目标

删除 UUT/设备/端口导致连线级联删除后，undo 恢复时保留 `PathStyle`。

### 修改范围

- `src/topology/UndoCommands.h`
- `src/topology/UndoCommands.cpp`
- `tests/topology/undo_commands_test.cpp`

### 修复

`ConnEntry` 增加：

```cpp
PathStyle style = PathStyle::Bezier;
```

保存时记录 `c->style`，恢复时赋回 `conn.style`。

### 提交建议

```txt
fix(topology): 恢复级联删除连线的样式信息
```

## Phase 8：修复用户可见 UI 问题

### 目标

修复低风险但用户能直接感知的问题。

### 修改范围

- `src/topology/TopologyEditorWidget.cpp`
- `src/topology/TopologyOutlineWidget.cpp`
- `src/topology/PropertyPanelWidget.cpp`
- `src/topology/TopologyView.cpp`

### 修复项

1. `paste_action_` 在复制成功后启用，或监听剪贴板状态。
2. `TopologyOutlineWidget::applyFilter()` 清空过滤时递归恢复所有子节点。
3. `PropertyPanelWidget::showPropertiesFor()` 离开设备页时去掉吞掉首次点击的 `return`。
4. 导出无后缀时根据 `selectedFilter` 补 `.png/.svg/.pdf`。
5. 主题切换后刷新 `TopologyView` 背景和图例。

### 提交建议

```txt
fix(topology): 修复拓扑编辑器若干可见交互问题
```

## Phase 9：移除 C++ 硬编码样式

### 目标

符合项目规则 10：Qt 样式统一放到 QSS。

### 修改范围

- `src/topology/TopologyEditorWidget.cpp`
- `src/app/resources/styles/*.qss`

### 修复

将 loading overlay 的 `setStyleSheet(...)` 迁移到 QSS。C++ 中只保留：

```cpp
loading_overlay_->setObjectName("PhLoadingOverlay");
label->setObjectName("PhLoadingOverlayLabel");
```

### 提交建议

```txt
style(topology): 将加载遮罩样式迁移到QSS
```

## Phase 10：增强导出失败处理

### 目标

让 SVG/PDF 写失败时正确返回 false，避免 UI 误报成功。

### 修改范围

- `src/topology/TopologySceneRenderer.cpp`
- `tests/topology/scene_renderer_test.cpp`

### 修复

SVG/PDF 使用显式 `QPainter::begin()`：

```cpp
QPainter painter;
if (!painter.begin(&gen)) {
  return false;
}
```

PDF 同理。

### 提交建议

```txt
fix(topology): 修复矢量导出失败状态判断
```

## Phase 11：增强模板和序列化健壮性

### 目标

修复不会立刻影响主流程，但会造成静默丢数据或坏文件误加载的问题。

### 修改范围

- `src/topology/DeviceTemplateManager.cpp`
- `src/topology/TopologyJsonSerializer.cpp`
- 对应 topology 单元测试

### 修复项

1. `DeviceTemplateManager` 保存/加载 `positionHint` 和 `portStyle`，并检查 `file.write()` 结果。
2. `TopologyJsonSerializer` 校验根对象、version、关键数组字段类型，失败时设置 `last_error_`。

### 提交建议

```txt
fix(topology): 增强模板和拓扑文件解析健壮性
```

## Phase 12：逐步结构拆分

### 目标

降低 `TopologyEditorWidget.cpp` 和 `PropertyPanelWidget.cpp` 的维护成本。该阶段不要和 bug 修复混在一起。

### 拆分顺序

1. `TopologyClipboard`：copy/paste、唯一名生成、内部连线恢复。
2. `TopologyExportController`：文件过滤器、后缀补全、调用 `TopologySceneRenderer`。
3. `TopologyCleanupDialog`：无效项列表和清理命令构建。
4. `TopologyAlignController`：对齐/分布 action 和命令。
5. `PropertyPanelWidget` 按页面拆分：
   - `UutPropertyPage`
   - `DevicePropertyPage`
   - `PortPropertyPage`
   - `DevicePortPropertyPage`
   - `ConnectionPropertyPage`
   - `MonitorPropertyPage`

### 提交建议

每拆一个独立模块一个提交，例如：

```txt
refactor(topology): 抽离拓扑剪贴板逻辑
refactor(topology): 抽离拓扑导出控制逻辑
refactor(topology): 拆分UUT属性页
```

## 推荐实际执行顺序

```txt
1. Phase 0 + Phase 1
   建测试，修撤销/重做删错对象。

2. Phase 2
   修重命名引用同步。

3. Phase 3 + Phase 4 + Phase 5
   修端口隐藏字段、无效 tap 清理、边界防御。

4. Phase 6 + Phase 7
   修样式撤销栈和级联恢复样式。

5. Phase 8 + Phase 9 + Phase 10
   修 UI 可见问题、QSS 规则、导出失败判断。

6. Phase 11
   修模板/序列化健壮性。

7. Phase 12
   再做结构拆分。
```

第一轮建议只执行 `Phase 0` 到 `Phase 5`，先把数据正确性问题修掉。
