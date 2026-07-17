# etlog 报告保存与分类标记方案

## 背景

1. **文件名覆盖**：`ExecutionPanelController` 中 etlog 文件名固定为 `程序名.etlog`，同一程序多次运行时后一次覆盖前一次
2. **分类缺乏标记**：`ProjectStructureWidget` 的"报告"分类中所有 etlog 文件平铺显示，无法一眼看出哪些是最新结果

## 目标

- 文件名带时间戳，保留历史报告
- 报告分类按修改时间降序，每个程序名的最新文件用 badge 标注

---

## Part 1：etlog 文件名加时间戳

### 改动文件

`src/app/ExecutionPanelController.cpp`

### 具体修改

1. 文件头追加 `#include <QDateTime>`
2. 第 251-253 行：

```cpp
// 改前：
QString etlog_path = report_dir + QStringLiteral("/") +
                     current_program_name_ +
                     QStringLiteral(".etlog");

// 改后：
QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
QString etlog_path = report_dir + QStringLiteral("/") +
                     current_program_name_ +
                     QStringLiteral("_") + timestamp +
                     QStringLiteral(".etlog");
```

### 效果

```
reports/综合Mock测试_20260717_143052.etlog
reports/综合Mock测试_20260717_093015.etlog
reports/基础测试_20260716_180000.etlog
```

### 兼容性

- 旧格式文件（无时间戳）不变，不会被影响
- etlog JSON 内部已有 `startTime` / `endTime` 字段，文件名时间戳仅用于文件系统层面区分

---

## Part 2：报告分类 badge 标记

### 新建文件

| 文件 | 说明 |
|------|------|
| `src/app/widgets/ProjectTreeDelegate.h` | 自定义树节点 delegate |
| `src/app/widgets/ProjectTreeDelegate.cpp` | 默认绘制 + 可选 badge 叠加 |

### 修改文件

| 文件 | 改动 |
|------|------|
| `src/app/ProjectStructureWidget.h` | 新增 `IsLatestRole` 枚举值 + delegate 成员 |
| `src/app/ProjectStructureWidget.cpp` | 创建/设置 delegate；`buildTree`/`refreshCategory` 中按时间排序并标记 |
| `src/app/CMakeLists.txt` | 加入新的 delegate 源文件 |

### 2.1 ProjectTreeDelegate 设计

继承 `QStyledItemDelegate`，仅对 `IsLatestRole == true` 的 item 叠加绘制 badge。

**paint() 伪逻辑**：

```
if (!index.data(IsLatestRole).toBool()) {
    return QStyledItemDelegate::paint(painter, option, index);
}

// 1. 右缩约 44px 给 badge 留空间
QStyleOptionViewItem opt = option;
const int kBadgeRightMargin = 48;
opt.rect.setWidth(opt.rect.width() - kBadgeRightMargin);

// 2. 默认绘制（icon + 文字 + 选中/悬停背景）
QStyledItemDelegate::paint(painter, opt, index);

// 3. 在缩出的区域画 badge
int badge_right = option.rect.right() - 4;
int badge_y = option.rect.top() + (option.rect.height() - badge_h) / 2;
// 绘制圆角矩形背景 + 文字
```

**Badge 样式**：

| 属性 | 值 |
|------|-----|
| 文本 | "最新" |
| 字体 | 8pt, bold |
| 背景半径 | 4px |
| 背景色（深色） | `QColor(76, 175, 80, 50)` |
| 背景色（浅色） | `QColor(76, 175, 80, 30)` |
| 文字色（深色） | `QColor(129, 199, 132)` |
| 文字色（浅色） | `QColor(56, 142, 60)` |
| 边距 | 左右 6px，上下 4px |

深浅判定沿用 `OpenFileDelegate` 的 `palette.color(QPalette::Base).lightness() < 128`。

### 2.2 ProjectStructureWidget 改动

#### 枚举扩展

```cpp
enum ProjectNodeRole {
  NodeTypeRole = Qt::UserRole + 1,
  RelativePathRole,
  CategoryIdRole,
  IsLatestRole,  // ← 新增，bool
};
```

#### initUi() 中创建 delegate

```cpp
tree_delegate_ = new ProjectTreeDelegate(this);
tree_view_->setItemDelegate(tree_delegate_);
```

#### buildTree() 中的报告分类处理

原代码段替换为：

#### 抽取 `populateReportCategory()` 消除重复

`buildTree()` 和 `refreshCategory()` 中的报告分类遍历逻辑完全相同，抽取为私有方法：

```cpp
// ProjectStructureWidget.h 新增声明：
void populateReportCategory(QStandardItem* catItem, const QFileInfoList& entries, const QString& dirPath);

// ProjectStructureWidget.cpp 实现：
void ProjectStructureWidget::populateReportCategory(
    QStandardItem* catItem,
    const QFileInfoList& entries,
    const QString& dirPath) {
  // 按修改时间降序排列
  QFileInfoList sorted = entries;
  std::sort(sorted.begin(), sorted.end(),
            [](const QFileInfo& a, const QFileInfo& b) {
              return a.lastModified() > b.lastModified();
            });

  QSet<QString> seenPrograms;
  QRegularExpression re(QStringLiteral("^(.+)_\\d{8}_\\d{6}\\.etlog$"));

  for (const auto& fi : sorted) {
    QString relPath = dirPath + fi.fileName();
    auto* item = createFileItem(fi.fileName(), relPath);

    // 只对 .etlog 文件做最新标记
    bool isLatest = false;
    if (fi.suffix().toLower() == "etlog") {
      QString programName;
      QRegularExpressionMatch m = re.match(fi.fileName());
      if (m.hasMatch()) {
        programName = m.captured(1);
      } else {
        programName = fi.completeBaseName();  // 旧格式
      }
      if (!seenPrograms.contains(programName)) {
        seenPrograms.insert(programName);
        isLatest = true;
      }
    }

    item->setData(isLatest, IsLatestRole);
    catItem->appendRow(item);
  }
}
```

#### buildTree() 中调用

将原报告分类处理代码段替换为：

```cpp
if (cat.id == "report") {
    populateReportCategory(catItem, entries, cat.dirPath);
} else {
    // 其他分类保持原样（字母序，无 badge）
    for (const auto& fi : entries) {
        QString relPath = cat.dirPath + fi.fileName();
        catItem->appendRow(createFileItem(fi.fileName(), relPath));
    }
}
```

#### refreshCategory() 中调用

原代码（第 675-693 行）：

```cpp
catItem->removeRows(0, catItem->rowCount());

QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
int fileCount = 0;
for (const auto& fi : entries) {
    QString relPath =
        QDir(project_path_).relativeFilePath(fi.absoluteFilePath());
    catItem->appendRow(createFileItem(fi.fileName(), relPath));
    ++fileCount;
}

QString baseName = catItem->data(Qt::DisplayRole).toString();
int parenIdx = baseName.indexOf(QStringLiteral(" ("));
if (parenIdx > 0) {
    baseName = baseName.left(parenIdx);
}
catItem->setText(baseName + QStringLiteral(" (") +
                 QString::number(fileCount) + QStringLiteral(")"));
```

改为：

```cpp
catItem->removeRows(0, catItem->rowCount());

QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
int fileCount = 0;

if (catId == "report") {
    populateReportCategory(catItem, entries,
                           QDir(project_path_).relativeFilePath(dirPath) +
                               QStringLiteral("/"));
    fileCount = catItem->rowCount();
} else {
    for (const auto& fi : entries) {
        QString relPath =
            QDir(project_path_).relativeFilePath(fi.absoluteFilePath());
        catItem->appendRow(createFileItem(fi.fileName(), relPath));
        ++fileCount;
    }
}

QString baseName = catItem->data(Qt::DisplayRole).toString();
int parenIdx = baseName.indexOf(QStringLiteral(" ("));
if (parenIdx > 0) {
    baseName = baseName.left(parenIdx);
}
catItem->setText(baseName + QStringLiteral(" (") +
                 QString::number(fileCount) + QStringLiteral(")"));
```

### 2.3 CMakeLists.txt

在 `src/app/CMakeLists.txt` 中 `widgets/OpenFileDelegate.cpp` 后追加：

```cmake
widgets/ProjectTreeDelegate.h
widgets/ProjectTreeDelegate.cpp
```

---

## 效果示意

```
报告 (4)
  综合Mock测试_20260717_143052.etlog  [最新 badge]
  综合Mock测试_20260717_093015.etlog
  基础测试_20260717_100000.etlog      [最新 badge]
  基础测试_20260716_180000.etlog
```

---

## 变更文件清单

| 文件 | 操作 |
|------|------|
| `src/app/widgets/ProjectTreeDelegate.h` | 新建 |
| `src/app/widgets/ProjectTreeDelegate.cpp` | 新建 |
| `src/app/ProjectStructureWidget.h` | 修改：枚举 + delegate 成员 + `populateReportCategory` 声明 |
| `src/app/ProjectStructureWidget.cpp` | 修改：delegate 创建/设置 + `populateReportCategory` 实现 + buildTree/refreshCategory 调用 |
| `src/app/ExecutionPanelController.cpp` | 修改：时间戳 + include |
| `src/app/CMakeLists.txt` | 修改：新文件追加 |
