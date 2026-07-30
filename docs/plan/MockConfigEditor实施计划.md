# MockConfigEditor 实施计划（阶段 5 T13-T18）

## 整体架构

MockConfigEditor 继承 QWidget + IEditor，布局 QSplitter：
- 左侧：QTreeWidget（UUT -> 端口 -> 响应/通道）
- 右侧：QStackedWidget（按选中节点类型切换编辑区）
- 注册：EditorManager registerExtension("emock","mockconfig") + registerFactory + binder

参考：ImageViewerWidget（骨架）+ TextEditorWidget（修改追踪 binder）

## T13 骨架

1. 新建 `src/app/editors/MockConfigEditor.{h,cpp}`
2. 类：`class MockConfigEditor : public QWidget, public IEditor` + Q_OBJECT
3. 构造：`MockConfigEditor(const QString& id, QWidget* parent)`
4. IEditor 实现：displayName/editorId/editorType("mockconfig")/filePath/widget/signalObject/isModified/save/saveAs/canUndo/canRedo/undo/redo/openFile
5. 基础布局：QSplitter + QTreeWidget(左) + QStackedWidget(右)
6. EditorManager.cpp：加 include + registerExtension("emock","mockconfig") + registerFactory(factory+binder)
7. src/app/CMakeLists.txt：加 MockConfigEditor.h/.cpp

## T14 左侧导航树

- openFile(filePath) 加载 MockResponses.emock + 同目录 topology.etopo
- 解析拓扑 JSON：products[] -> devices[] -> connections[]
- 构建树：UUT 节点 -> 端口节点（serial/can/a429/ad/da）-> 响应节点(帧型) / 通道节点(AD/DA)
- 节点数据存 UserRole：productName/deviceId/portName/mode 等

## T15 帧响应编辑区

- 回复帧选择 QComboBox（从 ICD 帧列表）
- ICD 字段表格 QTableWidget（字段路径/工程值/单位/范围/hex预览）
- 工程值编辑 -> buildFromIcd 构造 ResolvedSignal -> SignalCodec::encodeToFrame 编码 -> hex 预览实时更新
- 保存：fieldValues 写回 MockResponses.emock

## T16 AD 端口编辑区

- 模式切换 QRadioButton：固定值/波形/序列
- 固定值：QDoubleSpinBox
- 波形：类型下拉 + 幅值/频率/偏置 QDoubleSpinBox + 波形预览（QPainter 自绘，避免引入 QCustomPlot 到 app 层）
- 序列：QTableWidget（索引/值）+ 添加/删除按钮

## T17 DA 端口编辑区

- 仅 fixedValue QDoubleSpinBox

## T18 编辑器可见性与模式判断

- 项目打开时判断模式（拓扑设备 is_mock）
- 真实模式：入口隐藏
- Mock/空/混合模式：入口可见
- 复用 T5 的 is_mock 反查逻辑

## 实施顺序

T13（骨架）-> T14（导航树）-> T15/T16/T17（三个编辑区，可并行）-> T18（可见性）

## 依赖

- T15 依赖 T2（buildFromIcd）
- T16 依赖 T1（WaveformGenerator）
- 全部依赖 T13
