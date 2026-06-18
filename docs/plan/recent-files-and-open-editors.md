# 最近文件与已打开文件列表功能方案

## 背景

当前 `ProjectStructureWidget` 的占位页（无项目模式）只有"最近项目"列表，缺少"最近文件"入口。同时有项目模式下 `tree_view` 占满整个区域，用户无法看到当前已打开的文件列表（类似 VS Code 的 Open Editors）。

## 设计目标

| 模式 | 显示内容 | 数据来源 |
|---|---|---|
| 无项目（placeholder） | "最近文件" — 全局历史文件 | `CONFIG_RECENT_FILE_LIST` |
| 有项目（tree） | "已打开" — 当前编辑器打开的文件 | `EditorManager::openFiles()` |

## 实施分阶段

- **阶段一（当前）**：仅实现"已打开"列表（有项目模式）。独立完整、成本低、收益明确。
- **阶段二（后续）**：实现"最近文件"列表 + 项目检测对话框（无项目模式）。依赖用户实际无项目编辑频率评估。

---

## 阶段一：已打开文件列表

### 1. ProjectStructureWidget 改动

#### 1.1 tree 页面改为容器布局

将 tree_view 从直接加入 stack 改为容器：

```
tree_page (QWidget, QVBoxLayout)
├─ open_files_widget_ (QWidget, 可折叠，默认展开)
│   ├─ header行 (QHBoxLayout)
│   │   ├─ arrow_label_ (QLabel, "▶"/"▼")
│   │   ├─ "已打开" (QLabel, "PhOpenFilesHeader")
│   │   └─ count_label_ (QLabel, "(3)")
│   └─ open_files_container_ (QWidget, QVBoxLayout)
│       └─ RecentProjectCard × N (每行一个)
└─ tree_view_ (QTreeView, stretch=1)
```

#### 1.2 新增成员

- `QWidget* tree_page_` — 替代直接 addWidget(tree_view_)
- `QWidget* open_files_widget_`
- `QLabel* open_files_arrow_`
- `QWidget* open_files_container_`

#### 1.3 新增方法

- `void setOpenFiles(const QStringList& paths)` — 批量设置（项目打开时调用）
- `void onFileOpened(const QString& path)` — 增加单条
- `void onFileClosed(const QString& path)` — 移除单条
- `void toggleOpenFilesSection()` — 折叠/展开

#### 1.4 新增信号

- `void openFileActivateRequested(const QString& filePath)` — 点击已打开文件
- `void openFileCloseRequested(const QString& filePath)` — 右键关闭

#### 1.5 卡片复用 RecentProjectCard

- 传入：filePath、fileName、relativePath、""（隐藏时间）
- 调用方设 `setObjectName("PhOpenFileItem")` + `setAttribute(WA_StyledBackground)`
- `openRequested` → 发射 `openFileActivateRequested`
- `removeRequested` → 发射 `openFileCloseRequested`

### 2. EditorManager — 无需改动

已有全部所需 API：

- `openFiles()` — 返回当前打开的文件路径列表
- `fileOpened(filePath)` 信号 — 文件打开后发射
- `fileClosed(filePath)` 信号 — 文件关闭后发射
- `openFile(filePath)` — 已打开则 raise，未打开则创建新编辑器

### 3. main_window.cpp 连线

```cpp
// 项目打开时初始化列表（在 onProjectOpened 中调用）
psWidget->setOpenFiles(editor_manager_->openFiles());

// 文件打开/关闭 → 同步列表
connect(editor_manager_, &EditorManager::fileOpened,
        psWidget, &ProjectStructureWidget::onFileOpened);
connect(editor_manager_, &EditorManager::fileClosed,
        psWidget, &ProjectStructureWidget::onFileClosed);

// 项目关闭时清空列表（在 onProjectClosed 中调用）
psWidget->setOpenFiles({});

// 点击已打开文件 → 激活编辑器
connect(psWidget, &ProjectStructureWidget::openFileActivateRequested,
        this, [this](const QString& path) {
            editor_manager_->openFile(path);  // 已打开则 raise
        });

// 右键关闭已打开文件
connect(psWidget, &ProjectStructureWidget::openFileCloseRequested,
        this, [this](const QString& path) {
            editor_manager_->closeFile(path);
        });
```

### 4. QSS 样式

#### default.qss 追加

```css
/* --- Open Files section (tree mode) --- */
QLabel#PhOpenFilesHeader {
    color: #999999;
    font-size: 10px;
    font-weight: bold;
    text-transform: uppercase;
    letter-spacing: 1px;
    padding: 4px 0;
    background: transparent;
}
QWidget#PhOpenFileItem {
    background-color: transparent;
    border: none;
    border-radius: 4px;
    padding: 2px 4px;
}
QWidget#PhOpenFileItem:hover {
    background-color: #E8E8E8;
}
```

#### vscode.qss 追加

```css
/* --- Open Files section (tree mode) --- */
QLabel#PhOpenFilesHeader {
    color: #888888;
    font-size: 10px;
    font-weight: bold;
    text-transform: uppercase;
    letter-spacing: 1px;
    padding: 4px 0;
    background: transparent;
}
QWidget#PhOpenFileItem {
    background-color: transparent;
    border: none;
    border-radius: 4px;
    padding: 2px 4px;
}
QWidget#PhOpenFileItem:hover {
    background-color: #2A2D2E;
}
```

### 5. 涉及文件清单

| 文件 | 改动类型 |
|---|---|
| `src/app/ProjectStructureWidget.h` | 新增成员/槽/信号 |
| `src/app/ProjectStructureWidget.cpp` | 新增已打开 section、相关方法 |
| `src/app/main_window.cpp` | 新增连线 |
| `src/app/resources/styles/default.qss` | 追加样式 |
| `src/app/resources/styles/vscode.qss` | 追加样式 |
| `src/app/widgets/RecentProjectCard.h/.cpp` | 无改动（复用） |
| `src/app/EditorManager.h/.cpp` | 无改动 |
| `src/app/CMakeLists.txt` | 无改动（无新文件） |

### 6. 不做的事

- 不新建 `RecentFileCard` 控件（复用 `RecentProjectCard`）
- 不改 `EditorManager`（已有全部 API）
- 不在"已打开"列表显示文件修改状态（首版不做，后续可加 `*` 前缀）
- 不在"已打开"列表显示文件类型图标（首版不做）
- 不持久化折叠状态（首版默认展开）

### 7. 风险点

1. **项目切换时文件列表同步**：打开新项目前会关闭当前项目，`fileClosed` 信号会逐个触发 PSW 的 `onFileClosed`，列表自然清空。新项目打开后 `fileOpened` 逐个触发 `onFileOpened`。需确认这个时序不会出问题（可能需要先 `setOpenFiles({})` 再逐个加）。

---

## 阶段二：最近文件列表 + 项目检测（后续）

### 配置项（`src/core/config/ConfigDefs.h`）

```cpp
constexpr const char* CONFIG_RECENT_FILE_LIST = "recent/file_list";
constexpr const char* CONFIG_RECENT_FILE_TIMESTAMPS = "recent/file_timestamps";
constexpr int CONFIG_RECENT_FILE_MAX_COUNT = 15;
constexpr const char* CONFIG_RECENT_FILE_AUTO_OPEN_PROJECT = "recent/file_auto_open_project";
```

### placeholder 追加"最近文件"区域

在现有"最近项目"区域之后追加：

```
sc_layout (QVBoxLayout)
├─ ... (现有内容)
├─ recent_container_ (最近项目)
├─ sep3 (QFrame, "PhSeparator")          ← 新增
├─ "最近文件" (QLabel, "PhSectionLabel")  ← 新增
├─ recent_files_container_ (QWidget)     ← 新增，动态填充卡片
└─ addStretch()
```

### 记录最近文件（`main_window.cpp`）

```cpp
connect(editor_manager_, &EditorManager::fileOpened, this, [this](const QString& path) {
    auto& cfg = ConfigManager::instance();
    QStringList list = cfg.get<QStringList>(CONFIG_RECENT_FILE_LIST);
    list.removeAll(path);
    list.prepend(path);
    while (list.size() > CONFIG_RECENT_FILE_MAX_COUNT)
        list.removeLast();
    cfg.set(CONFIG_RECENT_FILE_LIST, list);

    QVariantMap timestamps = cfg.get<QVariantMap>(CONFIG_RECENT_FILE_TIMESTAMPS);
    timestamps[path] = QDateTime::currentDateTime();
    cfg.set(CONFIG_RECENT_FILE_TIMESTAMPS, timestamps);
});
```

### 点击最近文件的流程

```
1. 检查文件是否存在 → 不存在则提示并移除
2. findProjectFile(filePath) — 从文件目录向上遍历找 .etproj
   未找到 → 直接打开文件
3. 找到 → 读项目名 → 检查是否已是当前项目
   是 → 直接打开文件
4. 检查 CONFIG_RECENT_FILE_AUTO_OPEN_PROJECT
   true  → 直接打开项目 + 文件
   false → QMessageBox + QCheckBox 询问：
           "此文件属于项目 "xxx"，是否打开该项目？"
           ☑ 以后自动打开文件所属项目
           [打开项目] [仅打开文件]
```

### 风险点

- 向上遍历找 `.etproj` 可能误匹配（可接受，用户可选"仅打开文件"）
