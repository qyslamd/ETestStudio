# QAB 文件搜索框设计方案

## 背景

SARibbon 的 QuickAccessBar（QAB）位于标题栏左侧，当前放了新建项目、打开项目、保存、撤销、重做、登录、设置等按钮。用户希望在 QAB 末尾增加一个搜索框，用于按文件名搜索项目内文件，命中后通过 QCompleter 下拉展示匹配结果，用户点击某项后定位到项目树（`ProjectStructureWidget`）对应节点。

SARibbon 官方 example（`3rdparty/SARibbon-2.5.7/example/MainWindowExample/mainwindow.cpp:1912-1918`）已演示此用法：`quickAccessBar->addWidget(mSearchEditor)`。

## 现状分析

### QAB 当前布局

`MainWindow::setupRibbon()`（`MainWindow.cpp:2003-2061`）：

```
[新建项目] [打开项目] [保存] | [撤销] [重做] | [登录] [设置]
```

设置按钮之后无 separator，搜索框将放在设置按钮之后，新增一个 separator 隔开。

### ProjectStructureWidget 树结构

树模型（`QStandardItemModel* model_`）层级：

```
root (项目名)              <- NodeTypeRole = "root"
├── category (拓扑)         <- NodeTypeRole = "category"
│   ├── file (xxx.etopo)   <- NodeTypeRole = "file", RelativePathRole = "topology/xxx.etopo"
│   └── file (yyy.etopo)
├── category (报告)         <- NodeTypeRole = "category"
│   └── file (zzz.etlog)
├── category (硬件)
│   └── hardware_device     <- NodeTypeRole = "hardware_device"
└── ...
```

> 注意：树中还存在 `NodeTypeRole = "hardware_device"` 的节点，搜索只过滤 `"file"` 节点，不受影响。

文件项的 `Qt::DisplayRole` 存文件名（如 `xxx.etopo`），`RelativePathRole` 存相对路径。`model_->match()` 配合 `Qt::MatchContains | Qt::MatchRecursive` 可递归搜索。注意 `match()` 会同时命中 root/category/file/hardware_device 节点的 DisplayRole，需要用 `NodeTypeRole` 过滤。

### 关键 API

| 组件 | 方法 | 说明 |
|------|------|------|
| `SARibbonQuickAccessBar` | `addWidget(QWidget*)` | 向 QAB 添加任意 widget |
| `QCompleter` | `setModel()` / `setFilterMode(Qt::MatchContains)` | 自动过滤 model 数据，无需手动重建 |
| `QCompleter` | `activated(QString)` | 用户点击下拉项时触发 |
| `QStandardItemModel` | `match(startIndex, role, value, hits, flags)` | 递归搜索 model，`hits=-1` 取全部 |
| `QTreeView` | `setCurrentIndex()` | 选中节点 |
| `QTreeView` | `scrollTo(index, EnsureVisible)` | 滚动到可见 |
| `QTreeView` | `expand(parentIndex)` | 展开父级 |
| `QTreeView` | `clearSelection()` | 清除选中 |
| `SidebarWidget` | `switchPage(PageId::kProjectOverview)` | 切到项目概览页 |

## 设计决策

| 决策点 | 结论 | 理由 |
|--------|------|------|
| 搜索框位置 | QAB 末尾（设置按钮之后，新增 separator） | SARibbon API 原生支持，不侵入标题栏布局 |
| completer 数据源 | 项目打开时一次性填充 `allFileNames()`，`setFilterMode(MatchContains)` 让 completer 自动过滤 | 避免每次按键重建 model 导致下拉闪烁；completer 自带过滤无需手动搜索 |
| 触发方式 | `textChanged` 只处理清空选中；`activated` 处理定位 | 搜索过滤由 completer 内部完成 |
| 搜索范围 | 仅文件名（`Qt::DisplayRole`），递归全树收集 `NodeTypeRole=="file"` 节点 | 用户需求明确："搜索只搜文件名" |
| locateFile 匹配 | `Qt::MatchExactly`（精确匹配） | 用户从 completer 点击的是精确文件名，子串匹配会定位到错误文件 |
| 命中行为 | completer 下拉列表展示匹配文件名，用户点击某项后：切侧边栏到项目概览页 + 选中树节点 + 展开父级 + 滚动到可见 | 用户点击才切换，不打扰当前页面 |
| 清空搜索 | `tree_view_->clearSelection()` | 清除上次命中的选中状态 |
| 无项目时 | 搜索框始终 enable，`locateFile()` 内部返回 false | 用户可输入但无结果，无需 disable |
| 无命中时 | completer 下拉为空，不操作 | 静默不干扰 |
| 宽度 | 200px 固定 | QAB 空间有限，200px 够看文件名 |
| 父级展开 | 显式 `tree_view_->expand(target.parent())` | 不依赖 Qt 隐式行为，所有版本稳定 |

## 改动清单

### 1. MainWindow -- QAB 搜索框创建

`MainWindow.h` 新增成员和前向声明：

```cpp
class QLineEdit;
class QCompleter;
// ...
QLineEdit* ribbon_search_edit_ = nullptr;
QCompleter* ribbon_search_completer_ = nullptr;
```

`MainWindow.cpp` 新增 `#include <QLineEdit>`、`#include <QCompleter>` 和 `#include <QStringListModel>`。

`MainWindow.cpp` `setupRibbon()` 中 QAB 设置按钮之后（约 line 2061 后）：

```cpp
// ── QAB 文件搜索框 ──
qab->addSeparator();
ribbon_search_edit_ = new QLineEdit(this);
ribbon_search_edit_->setObjectName(QStringLiteral("RibbonSearchEdit"));
ribbon_search_edit_->setPlaceholderText(QStringLiteral("搜索文件..."));
ribbon_search_edit_->setFixedWidth(200);
ribbon_search_edit_->setClearButtonEnabled(true);
qab->addWidget(ribbon_search_edit_);

// QCompleter：项目打开时一次性填充文件名，completer 自动过滤
ribbon_search_completer_ = new QCompleter(ribbon_search_edit_);
ribbon_search_completer_->setCaseSensitivity(Qt::CaseInsensitive);
ribbon_search_completer_->setFilterMode(Qt::MatchContains);
ribbon_search_completer_->setModel(new QStringListModel(ribbon_search_completer_));
ribbon_search_edit_->setCompleter(ribbon_search_completer_);
```

### 2. MainWindow -- 信号连接

`MainWindow.cpp` `initSignalsLate()` 中添加：

```cpp
// QAB 搜索框：textChanged -> 清空时清除树上选中
connect(ribbon_search_edit_, &QLineEdit::textChanged, this, [this]() {
  if (ribbon_search_edit_->text().isEmpty()) {
    auto* psWidget = qobject_cast<ProjectStructureWidget*>(
        sidebar_->pageById(PageId::kProjectOverview));
    if (psWidget) {
      psWidget->clearTreeSelection();
    }
  }
});

// QAB 搜索框：completer activated -> 切侧边栏 + 定位树节点
connect(ribbon_search_completer_,
        QOverload<const QString&>::of(&QCompleter::activated), this,
        [this](const QString& fileName) {
  auto* psWidget = qobject_cast<ProjectStructureWidget*>(
      sidebar_->pageById(PageId::kProjectOverview));
  if (!psWidget) {
    return;
  }
  // 切到项目概览页 + 展开侧边栏
  sidebar_->switchPage(PageId::kProjectOverview);
  if (!sidebar_->isContentVisible()) {
    sidebar_->showContent();
  }
  activity_bar_->setActivePageId(PageId::kProjectOverview);
  // 定位文件（精确匹配）
  psWidget->locateFile(fileName);
});
```

### 3. MainWindow -- 项目打开/关闭时维护 completer

`MainWindow.cpp` `initSignalsLate()` 中新增两个独立 connect。

> **位置约束**：`projectOpened` connect **必须放在 `setProjectPath` connect（约 line 406-407）之后**，确保 `buildTree()` 先执行，否则 `allFileNames()` 返回空列表。

```cpp
// 项目打开时填充搜索框 completer（必须在 setProjectPath connect 之后）
connect(&projectMgr, &ProjectManager::projectOpened, this, [this]() {
  auto* psWidget = qobject_cast<ProjectStructureWidget*>(
      sidebar_->pageById(PageId::kProjectOverview));
  if (!psWidget) {
    return;
  }
  QStringList fileNames = psWidget->allFileNames();
  auto* completerModel = qobject_cast<QStringListModel*>(
      ribbon_search_completer_->model());
  if (completerModel) {
    completerModel->setStringList(fileNames);
  }
});

// 项目关闭时清空 completer model
connect(&projectMgr, &ProjectManager::projectClosed, this, [this]() {
  auto* completerModel = qobject_cast<QStringListModel*>(
      ribbon_search_completer_->model());
  if (completerModel) {
    completerModel->setStringList({});
  }
});
```

### 4. ProjectStructureWidget -- 新增方法

`ProjectStructureWidget.h` 新增 `#include <QStringList>` 和 public 方法：

```cpp
/// 收集树中所有文件节点的文件名
QStringList allFileNames() const;

/// 按文件名精确定位项目树节点（选中 + 展开 + 滚动）
/// @return true=命中并定位，false=未命中或无项目
bool locateFile(const QString& fileName);

/// 清除树选中状态
void clearTreeSelection();
```

`ProjectStructureWidget.cpp` 实现：

```cpp
QStringList ProjectStructureWidget::allFileNames() const {
  QStringList result;
  if (!root_item_ || project_path_.isEmpty()) {
    return result;
  }
  // 递归遍历树，收集所有 file 节点的文件名
  std::function<void(QStandardItem*)> collect;
  collect = [&](QStandardItem* item) {
    if (!item) {
      return;
    }
    if (item->data(NodeTypeRole).toString() == QStringLiteral("file")) {
      result << item->text();
    }
    for (int i = 0; i < item->rowCount(); ++i) {
      collect(item->child(i));
    }
  };
  collect(root_item_);
  return result;
}

bool ProjectStructureWidget::locateFile(const QString& fileName) {
  if (!root_item_ || project_path_.isEmpty()) {
    return false;
  }
  // 精确匹配文件名，取全部匹配后找第一个文件节点
  QModelIndex startIndex = model_->index(0, 0);
  QModelIndexList matches = model_->match(
      startIndex, Qt::DisplayRole, fileName, -1,
      Qt::MatchExactly | Qt::MatchRecursive);
  for (const auto& idx : matches) {
    if (idx.data(NodeTypeRole).toString() == QStringLiteral("file")) {
      // 显式展开父级 + 选中 + 滚动到可见
      tree_view_->expand(idx.parent());
      tree_view_->setCurrentIndex(idx);
      tree_view_->scrollTo(idx, QAbstractItemView::EnsureVisible);
      return true;
    }
  }
  return false;
}

void ProjectStructureWidget::clearTreeSelection() {
  tree_view_->clearSelection();
}
```

### 5. QSS 样式

`src/app/resources/styles/vscode.qss` 新增：

```css
/* QAB 搜索框 */
#RibbonSearchEdit {
    border: 1px solid #3C3C3C;
    border-radius: 2px;
    padding: 2px 6px;
    background: #2D2D2D;
    color: #CCCCCC;
    font-size: 12px;
}
#RibbonSearchEdit:focus {
    border-color: #007ACC;
}
```

`src/app/resources/styles/default.qss` 新增（浅色主题色值）：

```css
/* QAB 搜索框 */
#RibbonSearchEdit {
    border: 1px solid #CCCCCC;
    border-radius: 2px;
    padding: 2px 6px;
    background: #FFFFFF;
    color: #333333;
    font-size: 12px;
}
#RibbonSearchEdit:focus {
    border-color: #007ACC;
}
```

## 涉及文件汇总

| 文件 | 改动类型 |
|------|----------|
| `src/app/MainWindow.h` | 修改：新增 `QLineEdit*` + `QCompleter*` 成员 + 前向声明 |
| `src/app/MainWindow.cpp` | 修改：`#include <QLineEdit>` + `#include <QCompleter>` + `#include <QStringListModel>` + `setupRibbon()` 创建搜索框 + `initSignalsLate()` 连接 textChanged/activated + 项目打开/关闭时维护 completer |
| `src/app/ProjectStructureWidget.h` | 修改：`#include <QStringList>` + 新增 `allFileNames()` / `locateFile()` / `clearTreeSelection()` public 方法声明 |
| `src/app/ProjectStructureWidget.cpp` | 修改：`#include <functional>` + 实现三个方法 |
| `src/app/resources/styles/vscode.qss` | 修改：新增 `#RibbonSearchEdit` 样式 |
| `src/app/resources/styles/default.qss` | 修改：新增 `#RibbonSearchEdit` 样式 |

## 交互流程

```
用户在 QAB 搜索框输入文件名
         │
         ▼
   QCompleter 自动过滤（setFilterMode: MatchContains）
         │
         ▼
   下拉列表展示匹配文件名
         │
    ┌────┴────┐
    │         │
  有匹配    无匹配
    │         │
    ▼         ▼
 下拉列表   下拉为空
    │
    │ 用户点击某项
    ▼
 切侧边栏到项目概览页
         │
         ▼
 ProjectStructureWidget::locateFile(fileName)
 （MatchExactly 精确匹配）
         │
         ▼
 选中树节点 + 展开父级 + 滚动到可见

─── 清空搜索框 ───
         │
         ▼
   textChanged (text 为空)
         │
         ▼
   ProjectStructureWidget::clearTreeSelection()

─── 项目打开 ───
         │
         ▼
   ProjectStructureWidget::allFileNames()
         │
         ▼
   completer model 一次性填充
```

## 已知限制

1. **重名文件不可区分** -- 不同分类下存在同名文件时（如 `topology/test.etopo` 和 `backup/test.etopo`），completer 下拉列表显示两个相同的文件名，用户无法区分。点击后 `locateFile()` 只定位第一个匹配。后续可改为下拉项带相对路径（`RelativePathRole`）。

## 后续扩展方向（暂不实现）

1. **重名文件带路径** -- completer 下拉项显示 `topology/test.etopo` 而非 `test.etopo`，需要自定义 item delegate
2. **模糊匹配** -- 支持拼音首字母、通配符
3. **打开文件** -- completer 下拉项双击直接在编辑器打开（当前只定位项目树）
4. **搜索历史** -- 记录最近搜索词，下拉显示
5. **文件图标** -- QCompleter 下拉项带文件类型图标
