# EtlogViewerWidget 设计

## 背景

`.etlog` 是引擎执行 .etprog 后输出的 JSON 格式测试报告文件，目前仅在执行完成后写入磁盘，无任何可视化界面。

**需求**：在 ETestStudio 中以独立编辑器标签页打开 `.etlog` 文件，后验查看执行结果。

## 布局

```
┌─────────────────────────────────────────────────────────────┐
│  📊 测试报告: 燃油阀门控制       PASS: 5  FAIL: 2  ⏱: 3205ms │  <- SummaryBar
├──────────┬─────────────────────────┬────────────────────────┤
│  Cases   │     Steps               │     Step Detail        │
│          │                         │                        │
│  ✅ 用例1 │   ✅ SET 电压           │  Command:  SET         │
│  ❌ 用例2 │   ✅ CHECK 电流         │  Target:   CH1.A       │
│  ✅ 用例3 │   ❌ LOOP 5次           │  Status:   ❌ FAIL     │
│  ✅ 用例4 │     ├─ ✅ iteration 1   │  Expected: 25.0        │
│          │     │  ├─ ✅ SET 电流   │  Actual:   28.3        │
│          │     │  └─ ✅ CHECK 温度  │  Message:  超出容差    │
│          │     ├─ ❌ iteration 2   │  Elapsed:  45ms        │
│          │     │  ├─ ✅ SET 电流   │                        │
│          │     │  └─ ❌ CHECK 温度  │                        │
│          │     └─ ⏱ iteration 3..5 │                        │
│          │   ✅ IF                 │                        │
│          │     ├─ ✅ THEN: 打开阀门 │                        │
│          │     └─ (ELSE 未执行)     │                        │
├──────────┴─────────────────────────┴────────────────────────┤
│  执行时间: 2026-07-13T14:30:00  引擎版本: 1.0              │  <- FooterBar
└──────────────────────────────────────────────────────────────┘
```

## 架构

新文件 `src/app/editors/EtlogViewerWidget.h/.cpp`，实现 `IEditor` 接口。

```
EtlogViewerWidget (QWidget, implements IEditor)
├── SummaryBar (QFrame, top) — 套件名 + PASS/FAIL/ERROR 计数 + durationMs
├── QSplitter (horizontal, 三等分)
│   ├── CaseListPanel (QWidget)
│   │   └── QListWidget — 用例名 + status icon + durationMs（单行）
│   ├── StepTreePanel (QWidget)
│   │   └── QTreeView + QStandardItemModel — 4 列 [步骤/目标/状态/耗时]
│   │       ├── 普通 step → 单行
│   │       ├── LOOP/WHILE → 父节点 + iterations[].subSteps → 递归子节点
│   │       │   父节点 aggregated status: 子 FAIL→FAIL, 否则取首个非 PASS, 全 PASS→PASS
│   │       └── IF → 父节点 + branches.then[] / branches.else[] → 递归子节点
│   │           实际执行路径: status != PENDING 的分支, 另一条灰+"(未执行)"
│   └── StepDetailPanel (QScrollArea)
│       ├── Status badge (color-coded QLabel, 亮/暗两套色值, themeChanged 重置)
│       ├── Command / Target / Value
│       ├── Message (QTextEdit, read-only)
│       ├── Timestamp / ElapsedMs (纯毫秒整数)
│       ├── 始终显示所有标签, 无值显示 `-`
│       └── 纯平铺参数, 不负责嵌套展示
└── FooterBar (QFrame, bottom) — startTime / endTime / engineVersion
```

### Status 颜色定义

| 状态 | 亮主题 | 暗主题 |
|---|---|---|
| PASS | Green (#4CAF50) | Light green (#81C784) |
| FAIL | Red (#F44336) | Light red (#E57373) |
| ERROR | Dark red (#B71C1C) | Red (#EF5350) |
| TIMEOUT | Orange (#FF9800) | Orange (#FFB74D) |
| SKIPPED | Gray (#9E9E9E) | Gray (#BDBDBD) |
| PENDING | Light gray (#BDBDBD) | Dark gray (#616161) |

## IEditor 实现

- `editorType()` → `"etlog"`
- `displayName()` → `QFileInfo(filePath).fileName()`
- `isModified()` → `false`（只读）
- `save()` / `saveAs()` → `false`
- `undo()`/`redo()`/`canUndo()`/`canRedo()` → 无操作 / `false`
- `openFile(filePath)` → 读取 JSON → 解析 → 填充三层面板（构造只存 path + 建 UI）

## 编辑器注册

在 `EditorManager::registerEditorTypes()` 中追加：

```cpp
EditorFactoryRegistry::registerExtension("etlog", "etlog");
EditorFactoryRegistry::registerFactory(
    "etlog", [](const QString& id, QWidget* parent) {
      return new EtlogViewerWidget(id, parent);
    });
```

无需 binder（只读，无 modificationChanged 信号）。

## 数据流

1. `openFile()` → `QFile` 读取 JSON → `QJsonDocument` 解析
   - 解析失败 → 中央 QLabel 显示 `"无法解析测试报告"`，面板保持空白
   - 0 case / 0 step → 显示 `"文件中无执行结果"`
2. `SummaryBar`: 从 `summary` + `suiteName` 填充（PASS/FAIL/ERROR，无 TIMEOUT）
3. `CaseListPanel`: 遍历 `cases[]`，每个 item 持 caseIndex
4. 选中 case → 清空 StepTreePanel，递归遍历 `cases[caseIndex].steps[]`：
   - 普通 step → 单行（icon + command | target | status | elapsedMs）
   - LOOP/WHILE step → 父节点 + 遍历 `iterations[].subSteps` 递归建子树
     - 计算 aggregated status
   - IF step → 父节点 + 递归遍历 `branches.then[]` 和 `branches.else[]`
     - 判断实际执行分支（status != PENDING）
5. 选中任意树节点 → Detail 面板显示该节点的平铺参数

## 代码变更清单

| 文件 | 操作 |
|---|---|
| `src/app/editors/EtlogViewerWidget.h` | 新建 |
| `src/app/editors/EtlogViewerWidget.cpp` | 新建 |
| `src/app/EditorManager.cpp` | + 注册 |
| `src/app/CMakeLists.txt` | + 2 行 source |
