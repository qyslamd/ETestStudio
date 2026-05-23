# Bug 审计报告

> 审计日期：2026-05-23
> 当前分支：master
> 审计方式：多 agent 分模块代码审查

---

## 目录

1. [拓扑编辑器](#1-拓扑编辑器)
2. [帧协议编辑器](#2-帧协议编辑器)
3. [测试用例编辑器](#3-测试用例编辑器)
4. [文本编辑器 + QScintilla](#4-文本编辑器--qscintilla)
5. [QADS 使用](#5-qads-使用)

---

## 1. 拓扑编辑器

**文件路径：** `src/topology/`

### BUG-TOPO-001 — 产品端口 functionType 序列化丢失（数据损坏）

| 字段 | 值 |
|------|-----|
| **严重度** | 高 |
| **文件** | `TopologyJsonSerializer.cpp` 第 43-54 行（serialize）、第 164-174 行（deserialize） |
| **描述** | 产品端口（product ports）的 `functionType` 字段在序列化时未被写入 JSON，反序列化时也未读取。设备端口（device ports）则正确序列化了该字段（第 82-90 行）。保存再加载后，所有产品端口的 functionType 都会丢失，回退为默认值 `CUSTOM`。 |
| **修复** | serialize 中增加 `portObj["functionType"] = functionTypeToString(port.functionType)`；deserialize 中增加读取和转换。 |

### BUG-TOPO-002 — 设备端口删除不可撤销

| 字段 | 值 |
|------|-----|
| **严重度** | 中 |
| **文件** | `TopologyEditorWidget.cpp` 第 695-698 行 |
| **描述** | `onDeleteItem` 中删除 `DevicePortItem` 时直接调用 `doc_->removeDevicePort()` 修改文档，未通过撤销栈。其他所有删除操作（UUT、设备、连接、监听器）都使用 `QUndoStack::push()`。 |
| **修复** | 为设备端口删除创建 `RemoveDevicePortCommand` 或通过已有撤销命令封装。 |

### BUG-TOPO-003 — QTimer::singleShot 悬空指针风险

| 字段 | 值 |
|------|-----|
| **严重度** | 中 |
| **文件** | `topology_items.cpp` 第 314、447、933 行 |
| **描述** | `UutItem::onResizeFinished`、`DeviceItem::onResizeFinished`、`MonitorItem::onResizeFinished` 使用 `QTimer::singleShot(0, [doc, ...] {...})` 延迟推入撤销命令。如果编辑器在 timer 触发前销毁，`doc` 指针悬空，`doc->undoStack()` 是未定义行为。 |
| **修复** | 使用 `QPointer<TopologyDocument>` 或在 lambda 中捕获 `QPointer` / weak 指针检查有效性。 |

### BUG-TOPO-004 — 监听器视图状态丢失

| 字段 | 值 |
|------|-----|
| **严重度** | 低 |
| **文件** | `TopologyScene.cpp` 第 548-561 行 |
| **描述** | `clearScene()` 将 `monitor_view_active_` 硬重置为 `false`，每次撤销/重做/文件打开触发 `loadFromDocument()` 时监听器视图模式都被静默关闭，但工具栏按钮可能仍处于选中状态，导致 UI 不一致。 |
| **修复** | `clearScene()` 不应重置此状态，或在场景重建后从按钮状态恢复。 |

### BUG-TOPO-005 — Qt 6 兼容性问题

| 字段 | 值 |
|------|-----|
| **严重度** | 低 |
| **文件** | `PropertyPanelWidget.cpp` 第 584 行、`TopologyView.cpp` 第 273 行 |
| **描述** | `QString::SkipEmptyParts` 在 Qt 5.14+ 已弃用；`QContextMenuEvent::globalPos()` 已弃用。应分别替换为 `Qt::SkipEmptyParts` 和 `event->globalPosition().toPoint()`。 |

### BUG-TOPO-006 — PropertyPanelWidget 死代码

| 字段 | 值 |
|------|-----|
| **严重度** | 低 |
| **文件** | `PropertyPanelWidget.cpp` 第 648-653 行 |
| **描述** | `onDeviceTypeChanged()` 直接修改文档但不走撤销栈，且从未连接到任何信号（`device_type_edit_` 是只读的）。函数内容无效。 |

---

## 2. 帧协议编辑器

**文件路径：** `src/protocal/`

### BUG-PROTO-001 — smallint 值类型序列化数据损坏

| 字段 | 值 |
|------|-----|
| **严重度** | **严重** |
| **文件** | `ProtocalEditorWidget.cpp` 第 303 行（序列化）、第 35 行（反序列化） |
| **描述** | `smallint` 被序列化为 `"int16"`，但 `"int16"` 被反序列化为 `shortint`，导致 `smallint` → `shortint` 的静默类型转换。同样的问题也存在于 `icd_utility` 库自己的 JSON 序列化器/解析器中。xml_parser 中暂时不影响。 |
| **修复** | 序列化时 `smallint` 应输出特有标识（如 `"smallint"`），反序列化时增加对应分支。统一两套序列化器的值类型映射表。 |

### BUG-PROTO-002 — 属性面板大量 const_cast

| 字段 | 值 |
|------|-----|
| **严重度** | 高 |
| **文件** | `IcdPropertyPanel.cpp` 第 415-578 行 |
| **描述** | `current_node_` 声明为 `const icd::Node*`，但整个属性面板在所有编辑操作中都通过 C 风格 `const_cast<icd::Node*>(current_node_)` 绕过 const 正确性修改节点。`showNode()` 的入参也应为非 const 引用。 |
| **修复** | 将 `current_node_` 改为 `icd::Node*`，`showNode()` 改为接受 `icd::Node&`，移除所有 `const_cast`。 |

### BUG-PROTO-003 — 撤销/重做未实现

| 字段 | 值 |
|------|-----|
| **严重度** | 中 |
| **文件** | `ProtocalEditorWidget.cpp` 第 176-179 行 |
| **描述** | `canUndo()` / `canRedo()` 返回 `false`，`undo()` / `redo()` 是空函数。所有编辑操作（添加帧、删除帧、修改属性）均不可撤销。 |
| **修复** | 实现基于快照或命令模式的撤销/重做系统。 |

### BUG-PROTO-004 — 树控件无右键菜单

| 字段 | 值 |
|------|-----|
| **严重度** | 中 |
| **文件** | `IcdNodeTreeWidget.cpp` |
| **描述** | `IcdNodeTreeWidget` 设置 `NoEditTriggers` 但未设置 `setContextMenuPolicy`，用户无法通过右键菜单添加、删除或重命名帧/节点。 |
| **修复** | 添加 `Qt::CustomContextMenu` 策略和 `QMenu` 处理。 |

### BUG-PROTO-005 — 搜索框未连接过滤逻辑

| 字段 | 值 |
|------|-----|
| **严重度** | 低 |
| **文件** | `IcdNodeTreeWidget.cpp` 第 118-120 行 |
| **描述** | `filter_input_` 的 `textChanged` 信号从未连接，搜索框可见但不对树内容产生任何影响。 |
| **修复** | 连接 `textChanged` 信号到树节点过滤逻辑。 |

### BUG-PROTO-006 — 属性面板帧控件不活跃（误导）

| 字段 | 值 |
|------|-----|
| **严重度** | 中 |
| **文件** | `IcdPropertyPanel.cpp` 第 586-601 行 |
| **描述** | `showFrame()` 填充了 `spin_frame_id_`、`combo_frame_type_`、`combo_byte_order_` 但没有连接任何编辑信号。控件看起来可交互但修改无效。主工具栏中有另一组相同功能的控件（在 `ProtocalEditorWidget` 中连接正确）。 |
| **修复** | 移除面板中的帧级控件，或连接其信号使之生效。 |

### BUG-PROTO-007 — 帧删除时悬空指针窗口

| 字段 | 值 |
|------|-----|
| **严重度** | 中 |
| **文件** | `ProtocalEditorWidget.cpp` 第 502-514 行 |
| **描述** | `repo_.remove_frame(id)` 和 `setCurrentFrame(nullptr)` 之间，`current_frame_` 指向已被销毁的 `Frame`。虽然当前两行之间没有解引用，但脆弱。 |
| **修复** | 在 `remove_frame` 前先 `setCurrentFrame(nullptr)`。 |

### BUG-PROTO-008 — loadEproto 错误被静默忽略

| 字段 | 值 |
|------|-----|
| **严重度** | 中 |
| **文件** | `ProtocalEditorWidget.cpp` 第 181-187 行 |
| **描述** | `setEditorId()` 调用 `loadEproto(id)` 但忽略了返回值。文件损坏或格式错误时用户不会收到任何反馈。 |
| **修复** | 检查返回值并弹出错误提示。 |

### BUG-PROTO-009 — 已弃用 QWheelEvent::delta()

| 字段 | 值 |
|------|-----|
| **严重度** | 低 |
| **文件** | `IcdBitLayoutView.cpp` 第 338 行 |
| **描述** | `wheel->delta()` 自 Qt 5.0 弃用，Qt 6 移除。应使用 `wheel->angleDelta().y()`。 |

---

## 3. 测试用例编辑器

**文件路径：** `src/app/TestProgramEditorWidget.cpp` / `.h`、`TestProgramData.cpp` / `.h`

### BUG-TP-001 — 快照栈溢出导致 clean_snapshot_index_ 错位（数据丢失）

| 字段 | 值 |
|------|-----|
| **严重度** | **严重** |
| **文件** | `TestProgramEditorWidget.cpp` 第 222-225 行 |
| **描述** | 快照数量超过 `kMaxSnapshots`（100）时最早快照被移除，但 `clean_snapshot_index_` 未同步更新。如果它原指向 0（干净状态），移除后 `snapshots_[0]` 内容已变，但 `clean_snapshot_index_` 仍为 0。导致编辑器错误报告"已保存"状态，用户关闭时不提示保存，**数据丢失**。 |
| **修复** | 移除快照时同步更新 `clean_snapshot_index_`，如果丢弃了干净快照则置为 -1。 |

### BUG-TP-002 — 保存后清除全部撤销历史

| 字段 | 值 |
|------|-----|
| **严重度** | 中 |
| **文件** | `TestProgramEditorWidget.cpp` 第 348-351 行 |
| **描述** | `save()`/`saveAs()` 成功后调用 `snapshots_.clear()`，丢弃整个撤销历史。用户无法在保存后撤销保存前的操作。 |
| **修复** | 保存后不清除快照，仅更新 `clean_snapshot_index_` 为当前快照索引。 |

### BUG-TP-003 — 加载失败检测不可靠

| 字段 | 值 |
|------|-----|
| **严重度** | 中 |
| **文件** | `TestProgramEditorWidget.cpp` 第 323-325 行、`TestProgramData.cpp` 第 133-148 行 |
| **描述** | 以 `name.isEmpty() && cases.isEmpty()` 判断加载失败，但合法空文件也可能匹配此条件。另一方面，损坏 JSON 文件碰巧 name 非空时会被错误视为有效。`loadTestProgram()` 静默吞掉所有异常，调用方无法区分错误类型。 |
| **修复** | 使用 `tl::expected` 或返回错误信息字符串。 |

### BUG-TP-004 — blockSignals 不对称风险

| 字段 | 值 |
|------|-----|
| **严重度** | 中 |
| **文件** | `TestProgramEditorWidget.cpp` 第 186-193 行 |
| **描述** | `onAddStep()` 中使用 `table->blockSignals(true)` 后设为 `false`。如果外部代码在此之前已阻塞了表格信号，此处的 `blockSignals(false)` 会错误地恢复信号。 |
| **修复** | 保存旧状态：`bool wasBlocked = table->signalsBlocked()`，恢复时用 `blockSignals(wasBlocked)`。 |

### BUG-TP-005 — 加载时未保存当前 tab 选择

| 字段 | 值 |
|------|-----|
| **严重度** | 低 |
| **文件** | `TestProgramEditorWidget.cpp` 第 397-401 行 |
| **描述** | `loadProgramToUi()` 销毁并重建所有用例 tab，但没有记录和恢复之前选中的 tab 索引。撤销/重做后用户总被跳转到第一页。 |
| **修复** | `loadProgramToUi()` 开始前记录当前 tab 索引，重建后恢复。 |

### BUG-TP-006 — cases/ 目录不存在时创建失败

| 字段 | 值 |
|------|-----|
| **严重度** | 中 |
| **文件** | `TestProgramManagerWidget.cpp` 第 168-170 行 |
| **描述** | "新建用例"硬编码路径为 `cases/` 子目录。如果项目根目录下不存在 `cases/` 目录，`saveTestProgram()` 因目录不存在静默失败，用户收到通用错误难以诊断。 |
| **修复** | 保存前检查目录是否存在，不存在则自动创建。 |

---

## 4. 文本编辑器 + QScintilla

**文件路径：** `src/app/TextEditorWidget.cpp` / `.h`、`EditorManager.cpp` / `.h`

### BUG-TEXT-001 — saveAs 后 EditorManager 映射不同步（文件无法关闭）

| 字段 | 值 |
|------|-----|
| **严重度** | **严重** |
| **文件** | `EditorManager.cpp` 第 140-288 行、`main_window.cpp` 第 1007-1021 行 |
| **描述** | `saveAs` 修改了 `TextEditorWidget` 内部的 `file_path_`，但 `EditorManager` 的 `editors_` 和 `dock_widgets_` map 的 key 仍是旧路径。后续 `closeFile(editor->editorId())` 传入新路径，在 map 中找不到，文件无法关闭。 |
| **修复** | saveAs 成功后应调用 `updateEditorId()` 更新 EditorManager 的映射。 |

### BUG-TEXT-002 — Lexer 内存泄漏

| 字段 | 值 |
|------|-----|
| **严重度** | 高 |
| **文件** | `TextEditorWidget.cpp` 第 244-274 行 |
| **描述** | 每次 `setFilePath`/`saveFileAs` 调用 `applyLexer` 创建新 `QsciLexer*` 但不删除旧的。QsciScintilla 的 `setLexer()` 不接管所有权。每次重命名/另存为泄漏一个 lexer 对象。 |
| **修复** | `applyLexer` 中先 `delete editor_->lexer()` 再创建新的。 |

### BUG-TEXT-003 — 行尾符被静默转换（LF → CRLF）

| 字段 | 值 |
|------|-----|
| **严重度** | 高 |
| **文件** | `TextEditorWidget.cpp` 第 124-149 行 |
| **描述** | `loadFile` 使用 `QIODevice::Text` 打开文件，`\r\n` 被自动转为 `\n`。`saveFile` 使用 `QIODevice::Text` 写入，Windows 上 `\n` 被转回 `\r\n`。Unix (LF) 行尾的文件被静默转换为 CRLF。 |
| **修复** | 去掉 `QIODevice::Text` 标志，手动检测和保留文件原始行尾符。 |

### BUG-TEXT-004 — modificationChanged 信号双重发射

| 字段 | 值 |
|------|-----|
| **严重度** | 低 |
| **文件** | `TextEditorWidget.cpp` 第 44-54 行 |
| **描述** | 同时连接了 `QsciScintilla::modificationChanged` 和 `QsciScintilla::textChanged`，两个信号在 dirty 时先后触发，导致 `modificationChanged(true)` 被发射两次。 |
| **修复** | 移除 `textChanged` 连接，仅用 `modificationChanged` 一个信号源。 |

### BUG-TEXT-005 — onFileDeleted saveAs 空路径静默失败

| 字段 | 值 |
|------|-----|
| **严重度** | 高 |
| **文件** | `EditorManager.cpp` 第 696 行 |
| **描述** | 外部文件被删除且有未保存更改时，用户点击"保存"后调用 `editor->saveAs(QString())` 传递空路径。编辑器内部 `saveFileAs("")` 失败，不弹出文件选择对话框，用户以为保存成功实则什么都没发生。 |
| **修复** | 空路径时弹出 `QFileDialog::getSaveFileName` 让用户选择保存位置。 |

### BUG-TEXT-006 — onFileRenamed 仅处理 TextEditorWidget

| 字段 | 值 |
|------|-----|
| **严重度** | 中 |
| **文件** | `EditorManager.cpp` 第 719-721 行 |
| **描述** | 文件重命名时只有 `TextEditorWidget` 更新内部路径，`TopologyEditorWidget`、`ProtocalEditorWidget`、`TestProgramEditorWidget` 不处理，导致 `editor->filePath()` 返回旧路径。 |
| **修复** | 对其他编辑器类型也调用相应的 `setFilePath`/`setEditorId`。 |

### BUG-TEXT-007 — createEditor 缺少右键上下文菜单

| 字段 | 值 |
|------|-----|
| **严重度** | 中 |
| **文件** | `EditorManager.cpp` 第 487-591 行 |
| **描述** | `openFile()` 设置了 dock 的右键菜单（关闭/关闭其他/关闭右侧所有），但 `createEditor()` 完全缺失此设置。 |
| **修复** | 将右键菜单设置从 `openFile` 抽取到公共路径，或复制到 `createEditor`。 |

### BUG-TEXT-008 — 编辑操作仅支持 TextEditorWidget

| 字段 | 值 |
|------|-----|
| **严重度** | 中 |
| **文件** | `main_window.cpp` 第 1050-1170 行 |
| **描述** | 剪切/复制/粘贴/查找/替换/跳转行全部通过 `dynamic_cast<TextEditorWidget*>` 获取编辑器。其他编辑器类型激活时，这些菜单操作静默无响应。 |
| **修复** | 对非文本编辑器添加相应的处理分支（或禁用菜单项）。 |

### BUG-TEXT-009 — 替换操作逐个弹窗（用户体验差）

| 字段 | 值 |
|------|-----|
| **严重度** | 中 |
| **文件** | `main_window.cpp` 第 1131-1140 行 |
| **描述** | 查找替换循环中每个匹配项弹出一个 `QMessageBox::question`。100 个匹配项需要点击 100 次，没有"全部替换"选项。 |
| **修复** | 提供"全部替换"选项，或使用非模态查找替换面板。 |

### BUG-TEXT-010 — eventFilter ShortcutOverride 语义相反

| 字段 | 值 |
|------|-----|
| **严重度** | 低 |
| **文件** | `TextEditorWidget.cpp` 第 277-287 行 |
| **描述** | `ShortcutOverride` 中拦截 `Ctrl+S`/`Ctrl+W`，用 `ke->ignore()` 但语义应是 `ke->accept()`（当前因 `return true` 行为符合预期）。 |

---

## 5. QADS 使用

### BUG-QADS-001 — restoreState 时序导致编辑器布局丢失

| 字段 | 值 |
|------|-----|
| **严重度** | **高** |
| **文件** | `main_window.cpp` 第 209 行、第 72 行 |
| **描述** | `restoreWindowState()` 在 `restoreSession()` 之前调用，此时编辑器 dock widget 还未创建。QADS 保存的编辑器位置信息被丢弃，编辑器总是以默认位置打开。 |
| **修复** | 先创建所有编辑器 dock widget（空内容），再调用 `restoreState()`，最后加载内容。 |

### BUG-QADS-002 — closeFile 激活字母序第一个编辑器而非相邻

| 字段 | 值 |
|------|-----|
| **严重度** | 中 |
| **文件** | `EditorManager.cpp` 第 336-347 行 |
| **描述** | 关闭当前标签页后使用 `editors_.firstKey()` 激活下一个，`QMap::firstKey()` 是字母序，不是用户期望的相邻标签页。 |
| **修复** | 改为查找当前 dock 在 dock area 中相邻的标签页索引后激活。 |

### BUG-QADS-003 — editorById 空指针未检查

| 字段 | 值 |
|------|-----|
| **严重度** | 中 |
| **文件** | `EditorManager.cpp` 第 1411-1438 行 |
| **描述** | `session 恢复中`editorById(path)` 返回 `nullptr` 时未检查，后续 `dynamic_cast<TextEditorWidget*>(editor)` 可能收到空指针。虽然当前路径下编辑器已由 `openFile` 创建，但文件打开失败时可能崩溃。 |
| **修复** | 使用前检查 `editorById` 返回值。 |

### BUG-QADS-004 — saveState 和 captureSessionData 双重管理冲突

| 字段 | 值 |
|------|-----|
| **严重度** | 中 |
| **文件** | `main_window.cpp` 第 1208-1233 行、第 1257-1396 行 |
| **描述** | `saveState()` 保存所有 dock widget 的完整布局（位置+可见性），`captureSessionData()` 也保存面板/侧边栏可见性。恢复时可能冲突导致面板可见性不一致。 |
| **修复** | 统一状态管理，避免双重保存。 |

---

## 严重度分布

| 严重度 | 数量 | 问题列表 |
|--------|:----:|----------|
| **严重** | 3 | BUG-TP-001, BUG-PROTO-001, BUG-TEXT-001 |
| **高** | 6 | BUG-TOPO-001, BUG-PROTO-002, BUG-TEXT-002, BUG-TEXT-003, BUG-TEXT-005, BUG-QADS-001 |
| **中** | 14 | BUG-TOPO-002, BUG-TOPO-003, BUG-PROTO-003, BUG-PROTO-004, BUG-PROTO-006, BUG-PROTO-007, BUG-PROTO-008, BUG-TP-002, BUG-TP-003, BUG-TP-004, BUG-TP-006, BUG-TEXT-006, BUG-TEXT-007, BUG-TEXT-008, BUG-TEXT-009, BUG-QADS-002, BUG-QADS-003, BUG-QADS-004 |
| **低** | 7 | BUG-TOPO-004, BUG-TOPO-005, BUG-TOPO-006, BUG-PROTO-005, BUG-PROTO-009, BUG-TP-005, BUG-TEXT-004, BUG-TEXT-010 |
