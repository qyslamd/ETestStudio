# TopologyManagerWidget 设计规格

## 背景
activity bar 的「拓扑」按钮对应侧边栏是一个空的 `new QWidget(sidebar_)`，点击后什么也不显示。
需要一个完整的拓扑管理器，列出项目的 `.etopo` 文件，支持预览和搜索，并集成到 SidebarWidget。

## 架构

```
TopologyManagerWidget : QWidget
├── [搜索框] QLineEdit — 实时过滤文件/设备列表
│   └── 200ms 防抖 QTimer + setClearButtonEnabled(true)
├── [工具栏] "+ 新建" QPushButton
└── [文件树] QTreeWidget
    ├── 文件A.etopo
    │   ├── CAN设备 (2CH, IN)
    │   ├── 仿真设备1 (4CH, OUT)
    │   └── UUT: 被测对象1
    ├── 文件B.etopo
    │   └── ...
    └── ...
```

### 功能
- 文件列表：`refreshList()` 从 `scanDirectory("topology", "etopo")` 获取文件列表
- 拓扑预览：解析每个 `.etopo` JSON 文件，提取设备/UUT 信息显示为子节点
- 搜索过滤：实时匹配文件名 + 全部设备属性文本
- 新建/重命名/删除：文件级 CRUD 操作
- 双击文件：通过 `openFileRequested` 信号在编辑器区打开
- 文件监控联动：`ProjectStructureWidget` 的 `directoryContentChanged` 信号自动触发刷新

## 文件改动清单

### 新增文件

#### `src/app/TopologyManagerWidget.h`
```cpp
namespace etest::app {

class TopologyManagerWidget : public QWidget {
    Q_OBJECT
public:
    explicit TopologyManagerWidget(QWidget* parent = nullptr);

public slots:
    void refreshList();

signals:
    void openFileRequested(const QString& filePath);

private:
    void setupUi();
    void initSignals();

    // 树操作
    void onItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onCustomContextMenu(const QPoint& pos);

    // 文件操作
    void onNewTopology();
    bool renameTopologyFile(const QString& oldPath);
    bool removeTopologyFile(const QString& filePath);

    // 解析：读取 .etopo JSON，生成设备/UUT/监视器/连接预览子节点
    void addPreviewNodes(QTreeWidgetItem* fileItem, const QString& absPath);

    // 搜索
    void applySearchFilter(const QString& keyword);

    QLineEdit* search_edit_ = nullptr;
    QTimer* search_timer_ = nullptr;
    QPushButton* new_btn_ = nullptr;
    QTreeWidget* tree_ = nullptr;
};
}
```

#### `src/app/TopologyManagerWidget.cpp`

##### setupUi()
- 垂直布局，无边距
- 顶部：`QLineEdit`（placeholder "搜索拓扑文件…"、clearButton）
- 中部：`+ 新建` 按钮（24px 高，同类管理器风格）
- 底部：`QTreeWidget`（隐藏表头、右键菜单、双击展开关闭）

##### initSignals()
- `tree_::itemDoubleClicked` → `onItemDoubleClicked`
- `tree_::customContextMenuRequested` → `onCustomContextMenu`
- `new_btn_::clicked` → `onNewTopology`
- `search_edit_::textChanged` → `search_timer_->start(200)`（防抖，每次输入重置计时）
- `search_timer_::timeout` → `applySearchFilter(search_edit_->text())`

##### refreshList()
```
clear tree
if not projectOpen → return
paths = project->scanDirectory("topology", "etopo")
for each path:
    item = new QTreeWidgetItem
    item->setText(0, QFileInfo(path).completeBaseName())
    item->setData(0, UserRole, path)
    item->setToolTip(0, path)
    addPreviewNodes(item, path)  // 解析JSON加设备/UUT/监视器子节点
tree_->expandAll()
```

##### addPreviewNodes(fileItem, absPath)
读取 absPath 的 JSON，解析 4 个顶层数组：

| 数组 | 子节点标签格式 | 数据来源 |
|------|---------------|---------|
| `products[]` | `"UUT: {name}"` | 每个 product 的 `name` 字段 |
| `devices[]` | `"{name} ({deviceType})"` | 每个 device 的 `name` + `deviceType` |
| `monitors[]` | `"监视: {name} ({deviceType}, {channelCount}CH)"` | 每个 monitor 的 `name` + `deviceType` + `channelCount` |
| `connections[]` | `"连接: {device} → {product}.{port}"` | 每个 connection 的 `device` + `product` + `port` |

解析失败 → 灰色子节点 "(解析失败)"。

##### 搜索过滤：applySearchFilter(keyword)
```
if keyword.isEmpty:
    全部显示，return

for 每个顶层文件节点:
    fileMatch = 文件名 contains keyword
    childMatch = false
    for 每个子节点:
        matches = 子节点文本 contains keyword
        childMatch |= matches
        child->setHidden(!matches)
    fileItem->setHidden(!fileMatch && !childMatch)
```

##### 文件操作
- `onNewTopology()`:
  QInputDialog → `topology/<name>.etopo` → 空 JSON 结构 → refreshList → openFileRequested
- `renameTopologyFile(oldPath)`:
  QInputDialog → QFile::rename → refreshList
- `removeTopologyFile(filePath)`:
  QMessageBox 确认 → QFile::remove → refreshList

### 修改文件

#### `src/app/SidebarWidget.h`
- 添加 `class TopologyManagerWidget;` 前置声明
- 添加 `TopologyManagerWidget* topology_manager_ = nullptr;`
- 添加 `TopologyManagerWidget* topologyManager() const;`

#### `src/app/SidebarWidget.cpp`
- 添加 `#include "TopologyManagerWidget.h"`
- `addPage()` 的 qobject_cast 链追加 `TopologyManagerWidget`
- 实现 `topologyManager()` accessor

#### `src/app/main_window.cpp`
- 添加 `#include "TopologyManagerWidget.h"`
- `lazyInit()`：`new QWidget(sidebar_)` → `new TopologyManagerWidget(sidebar_)`
- `initSignalsLate()`：
  - 连接 `openFileRequested` → `editor_manager_->openFile`
  - 在 `directoryContentChanged` lambda 中补充 `topology/` 路由 → `topologyManager()->refreshList()`

### 不改动的文件
- `ActivityBarWidget` — 拓扑按钮已注册，图标 "topo_tap" 正常
- `EditorManager` — 已有拓扑编辑器工厂
- `TopologyEditorWidget` — 不需修改
- `ProjectStructureWidget` — 拓扑文件已在树中正确显示

## 新文件格式
创建空 `.etopo` 文件时写入：
```json
{
  "version": 1,
  "products": [],
  "devices": [],
  "connections": [],
  "monitors": []
}
```
与 `TopologyJsonSerializer::serialize()` 输出的结构一致。

## 验证
1. 构建：`scripts/build_ninja.bat -t debug -m ETestStudio`
2. 运行，打开项目 → 点击「拓扑」→ 显示文件列表
3. 双击 `.etopo` 文件 → 在编辑区打开拓扑编辑器
4. 点「+ 新建」→ 输入名称 → 文件创建、列表刷新、编辑器打开
5. 右键重命名/删除 → 操作成功
6. 搜索框输入 → 实时过滤文件名和设备名
7. 在项目目录 `topology/` 中增删文件 → 列表自动刷新
