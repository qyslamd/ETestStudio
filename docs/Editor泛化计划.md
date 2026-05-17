# Editor 泛化计划

## 目的

当前 `EditorManager` 与 `EditorWidget`（QsciScintilla 文本编辑器）是强耦合的：

- `EditorManager` 存储 `QMap<QString, EditorWidget*>`，key 为文件路径
- `EditorWidget` 构造函数直接创建 `QsciScintilla`，没有抽象层
- `TopologyEditorWidget` 是完全独立的 QWidget，无法通过 EditorManager 统一管理

引入 IEditor 纯虚接口后，编辑器体系变为可扩展的：

```
IEditor
├── TextEditorWidget (QsciScintilla 文本编辑器)
│   └── .cpp, .h, .py, .json, .lua ...
└── TopologyEditorWidget (图形化拓扑编辑器)
    └── .ettopo / 新建拓扑
```

## 注意：`filePath()` vs `editorId()` 的 key 冲突

`IEditor` 同时提供 `filePath()` 和 `editorId()`：
- 文本编辑器：两者返回相同值（文件路径）
- 拓扑编辑器（未保存）：`filePath()` → `""`, `editorId()` → `"editor://topology/new"`
- 拓扑编辑器（已保存）：两者返回相同值（文件路径）

**EditorManager 的内部 map key 统一使用 `editorId()`**。所有外部传入的"文件路径"类参数在用于 map 查找前，需确认是 editor ID 而非 file path。对于 `closeFile()`，其语义已变为"按 editor ID 关闭编辑器"。

## 目标

1. **解耦**：EditorManager 不再依赖具体编辑器类型，只面向 IEditor 接口编程
2. **可扩展**：新增编辑器类型只需实现 IEditor + 注册工厂，无需修改 EditorManager
3. **归一的编辑器管理**：所有编辑器（文本/拓扑/未来其他）共享 dock tab、脏标记、保存/关闭流程
4. **静态工厂**：通过文件后缀或编辑器类型名路由到正确的编辑器实现

---

## 执行计划

### 阶段一：创建 IEditor 纯虚接口

**新建 `src/app/editor/IEditor.h`**

```cpp
#pragma once

#include <QString>
#include <QWidget>

namespace etest::app {

class IEditor {
 public:
  virtual ~IEditor() = default;

  virtual QString displayName() const = 0;
  virtual bool isModified() const = 0;
  virtual bool save() = 0;
  virtual bool saveAs(const QString& path) = 0;
  virtual QString filePath() const = 0;
  virtual QString editorId() const = 0;    // 唯一 key，用于 EditorManager 的 map
  virtual QWidget* widget() = 0;           // 返回实际 widget 指针
  virtual QString editorType() const = 0;  // "text", "topology", ...
  virtual QObject* signalObject() = 0;     // 返回 QObject* 用于 connect
};

}  // namespace etest::app
```

不含 `Q_OBJECT`，不继承 `QObject`，每个实现类在各自的 `QWidget` 上声明信号。

### 阶段二：创建静态工厂

**新建 `src/app/editor/EditorFactory.h`**

```cpp
#pragma once

#include <QString>
#include <QMap>
#include <functional>
#include "IEditor.h"

namespace etest::app {

using EditorFactory = std::function<IEditor*(const QString& id, QWidget* parent)>;

class EditorFactoryRegistry {
 public:
  static void registerFactory(const QString& editorType, EditorFactory factory);
  static void registerExtension(const QString& suffix, const QString& editorType);
  static IEditor* create(const QString& editorType, const QString& id,
                         QWidget* parent = nullptr);
  static QString typeForExtension(const QString& suffix);

 private:
  static QMap<QString, EditorFactory>& factories();
  static QMap<QString, QString>& extensionMap();
};

}  // namespace etest::app
```

**新建 `src/app/editor/EditorFactory.cpp`** — 实现两个 static QMap + 增删查方法。

### 阶段三：重命名 EditorWidget → TextEditorWidget

| 操作 | 文件 |
|---|---|
| 移动 | `EditorWidget.h` → `TextEditorWidget.h` |
| 移动 | `EditorWidget.cpp` → `TextEditorWidget.cpp` |
| 类改名 | `EditorWidget` → `TextEditorWidget : public QWidget, public IEditor` |
| 实现接口 | `editorId()` → `file_path_`、`editorType()` → `"text"`、`signalObject()` → `this`、`widget()` → `this`、`displayName()` → 调用已有 `fileName()` |
| 保留 | 全部现有方法（`editor()` → QsciScintilla*、`save()`、`saveAs()` 等） |

### 阶段四：TopologyEditorWidget 适配 IEditor

修改 `TopologyEditorWidget.h`：
- 改为 `: public QWidget, public IEditor`
- 添加 IEditor 全部纯虚方法声明
- 添加 `Q_SIGNALS: void modificationChanged(bool modified);`
- **添加 `Q_SIGNALS: void editorIdChanged(const QString& oldId, const QString& newId);`** — 保存新拓扑后通知 EditorManager 更新 map key
- **新增 `TopologyDocument* document() const;`** 公开方法，返回 `doc_`（供工厂和外部使用）
- **新增 `void reloadScene();`** 公开方法，调用 `scene_->loadFromDocument()`（供工厂加载文件后刷新视图）

修改 `TopologyEditorWidget.cpp`：
- `editorId()` → 有文件时返回 `current_file_`，无文件时返回 `"editor://topology/new"` 合成 ID。当用户保存后调用 `setEditorId(newPath)` 同步更新 ID
- `isModified()` → 返回 `doc_->isModified()`
- `save()` → 复用已有的 `onSaveFile()` 逻辑（JSON 序列化）; 保存成功后调用 `doc_->setModified(false)` 并 emit `modificationChanged(false)`
- `saveAs(path)` → 复用已有的 `onSaveAsFile()` 逻辑; 保存成功后更新 `editorId` 为 `path`，并通过信号通知 EditorManager 更新 map key
- `displayName()` → `current_file_.isEmpty() ? "硬件拓扑(未保存)" : QFileInfo(current_file_).fileName()`
- `editorType()` → `"topology"`
- `signalObject()` → `this`
- `widget()` → `this`

修改 `TopologyDocument.h/.cpp`：
- 新增 `bool modified_` 字段，默认 `false`
- 新增 `void setModified(bool)` / `bool isModified() const`
- 每个 `add*()` / `remove*()` / `clear()` 操作末尾调用 `setModified(true)`
- **每个 `deviceChanged()` / `productChanged()` 信号发射处也调用 `setModified(true)`**（因为这些信号由属性面板编辑触发，直接在文档结构体上修改数据，不经过 add/remove 方法）。在 `addDevicePort()` / `removeDevicePort()` 中也调用 `setModified(true)`

### 阶段五：EditorManager 改造

| 改动 | 说明 |
|---|---|
| `QMap<QString, EditorWidget*>` → `QMap<QString, IEditor*>` | 内部存储改为 IEditor 指针，key 为 `editor->editorId()` |
| `EditorWidget* editorForFile()` → `IEditor* editorById()` | 返回类型和命名变化 |
| `currentEditor()` → `IEditor*` | 返回类型变化 |
| signal: `currentEditorChanged(IEditor*)` | 参数类型变化（直接连接可用，同一线程无需注册元类型） |
| 新增 `createEditor(type, id, title)` | 无文件编辑器入口 |
| `openFile(path)` 内创建编辑器 | 改为通过工厂创建：查扩展名 → 查工厂 → `create()` |
| `openFile()` 内信号连接 | 通过 `editor->signalObject()` + `dynamic_cast` 连接。**补充 `editorIdChanged` 连接** |
| **新增 `updateEditorId(IEditor*, const QString& newId)`** | 编辑器保存后 ID 变化时更新两个 map 的 key |
| **`openFileAtLine()` 加 dynamic_cast** | `editor->editor()` → `dynamic_cast<TextEditorWidget*>(editor)->editor()` |
| **`updateDockTitle()` 参数类型 + 方法名** | `EditorWidget*` → `IEditor*`; `editor->fileName()` → `editor->displayName()` |
| **全文件替换 `saveFile()` → `save()`** | `closeFile()` 第 139 行、`saveAllFiles()` 第 187 行、`saveModifiedFiles()` 第 214 行、`saveModifiedFilesInDirectory()` 第 237 行、`onFileDeleted()` 第 352 行 |
| **全文件替换 `saveFileAs()` → `saveAs()`** | `onFileDeleted()` 第 352 行 |
| **全文件替换 `editor->fileName()` → `editor->displayName()`** | `closeFile()` 第 133 行、`onFileDeleted()` 第 342 行 |

`openFile()` 中信号连接示意：

```cpp
auto* editor = EditorFactoryRegistry::create(type, filePath, nullptr);
auto* obj = editor->signalObject();

if (auto* textEditor = dynamic_cast<TextEditorWidget*>(editor)) {
  connect(textEditor, &TextEditorWidget::modificationChanged, ...);
} else if (auto* topoEditor = dynamic_cast<TopologyEditorWidget*>(editor)) {
  connect(topoEditor, &TopologyEditorWidget::modificationChanged, ...);
  connect(topoEditor, &TopologyEditorWidget::editorIdChanged,
          this, &EditorManager::updateEditorId);
}
```

**`onFileDeleted()` 需补充 `dynamic_cast`**：原代码第 359 行 `editor->editor()->setModified(false)` 直接操作 QsciScintilla，改为：

```cpp
if (ret == QMessageBox::Discard) {
  if (auto* textEditor = dynamic_cast<TextEditorWidget*>(editor)) {
    textEditor->editor()->setModified(false);
  }
  // 拓扑编辑器丢弃更改不需要额外操作（文档已留在内存中）
}
```

**`onFileRenamed()` 需补充 `dynamic_cast`**：原代码第 377 行 `editor->setFilePath(newPath)` 是 TextEditorWidget 特有方法，改为：

```cpp
if (auto* textEditor = dynamic_cast<TextEditorWidget*>(editor)) {
  textEditor->setFilePath(newPath);
}
// 更新 map key
editors_.remove(oldPath);
editors_[newPath] = editor;
```

**编辑器 ID 生命周期管理**：当拓扑编辑器的 `saveAs()` 被调用后，`current_file_` 从空变为文件路径，此时 `editorId()` 从 `"editor://topology/new"` 变为实际路径。`TopologyEditorWidget::saveAs()` 应在保存成功后 emit 一个信号（如 `editorIdChanged(oldId, newId)`），EditorManager 接收后调用 `updateEditorId()` 更新两个 map 的 key。

**`closeFilesInDirectory()` / `hasUnsavedChangesInDirectory()` 路径过滤**：当前使用 `dir.relativeFilePath(it.key())` 判断文件是否在目录内。对于合成 ID `"editor://topology/new"`，跨盘符（Windows）时 `relativeFilePath()` 不会以 `".."` 开头，导致拓扑编辑器被意外纳入目录操作范围。修正：在 `closeFilesInDirectory()` 和 `hasUnsavedChangesInDirectory()` 中添加编辑器类型检查，仅处理 `editorType() == "text"` 的编辑器。

### 阶段六：MainWindow 适配

`currentEditor()` 返回 `IEditor*` 后，所有调用 `.editor()`（QsciScintilla*）的地方必须加 `dynamic_cast<TextEditorWidget*>`。

涉及的主要区域：

| 位置 | 改动 |
|---|---|
| `initSignals()` 中 `[this](EditorWidget* editor)` lambda | 参数改为 `IEditor*`，内部 `editor->editor()` → `dynamic_cast<TextEditorWidget*>(editor)->editor()`；`editorStateChanged` 连接移入 `dynamic_cast<TextEditorWidget*>` 分支内 |
| `onUndo()` / `onRedo()` / `onCut()` / `onCopy()` / `onPaste()` | 加 dynamic_cast |
| `onFind()` / `onReplace()` / `onGoToLine()` | 加 dynamic_cast |
| `captureSessionData()` / `restoreSession()` | 加 dynamic_cast |
| `onSaveFile()` / `onSaveFileAs()` | `editor->saveFile()` → `editor->save()` / `editor->saveFileAs()` → `editor->saveAs()` |
| `onCloseCurrentFile()` | `editor->filePath()` → `editor->editorId()`（因为 EditorManager 内部 map 以 editorId 为 key） |

### 阶段七：main.cpp 注册工厂

在 `main()` 中 `MainWindow main_window;` 之前添加：

```cpp
#include "editor/EditorFactory.h"
#include "TextEditorWidget.h"
#include "topology/TopologyEditorWidget.h"
#include "topology/TopologyJsonSerializer.h"

// 注册后缀映射（文本编辑器）
EditorFactoryRegistry::registerExtension("cpp", "text");
EditorFactoryRegistry::registerExtension("h", "text");
EditorFactoryRegistry::registerExtension("hpp", "text");
EditorFactoryRegistry::registerExtension("c", "text");
EditorFactoryRegistry::registerExtension("cc", "text");
EditorFactoryRegistry::registerExtension("cxx", "text");
EditorFactoryRegistry::registerExtension("py", "text");
EditorFactoryRegistry::registerExtension("lua", "text");
EditorFactoryRegistry::registerExtension("json", "text");
EditorFactoryRegistry::registerExtension("xml", "text");
EditorFactoryRegistry::registerExtension("html", "text");
EditorFactoryRegistry::registerExtension("yaml", "text");
EditorFactoryRegistry::registerExtension("yml", "text");
EditorFactoryRegistry::registerExtension("md", "text");
EditorFactoryRegistry::registerExtension("js", "text");
EditorFactoryRegistry::registerExtension("cmake", "text");
EditorFactoryRegistry::registerExtension("txt", "text");

// 注册工厂
EditorFactoryRegistry::registerFactory("text", [](const QString& path, QWidget* parent) {
  return new TextEditorWidget(path, parent);
});

EditorFactoryRegistry::registerFactory("topology", [](const QString& id, QWidget* parent) {
  auto* editor = new TopologyEditorWidget(parent);
  // 如果 id 是实际文件路径（非合成 ID），加载文件内容
  if (!id.startsWith("editor://") && QFileInfo::exists(id)) {
    QFile file(id);
    if (file.open(QIODevice::ReadOnly)) {
      QJsonParseError err;
      QJsonDocument jdoc = QJsonDocument::fromJson(file.readAll(), &err);
      file.close();
      if (err.error == QJsonParseError::NoError) {
        TopologyJsonSerializer::deserialize(jdoc.object(), editor->document());
        editor->document()->setModified(false); // 刚加载，未修改
        // 刷新场景视图
        editor->reloadScene();
      }
    }
  }
  return editor;
});
```

### 阶段八：拓扑入口重新接入（未来）

未来重新添加拓扑按钮时：

```cpp
connect(topologyAction, &QAction::triggered, this, [this]() {
  editor_manager_->createEditor("topology",
                                "editor://topology/new",
                                "硬件拓扑");
});
```

### 阶段九：CMakeLists.txt 更新

修改 `src/app/CMakeLists.txt`：

- 添加 `src/app/editor/IEditor.h` 到 target header 列表（header-only，无需加入 SOURCES）
- 添加 `editor/EditorFactory.h` 和 `editor/EditorFactory.cpp` 到 SOURCES
- `EditorWidget.h EditorWidget.cpp` → `TextEditorWidget.h TextEditorWidget.cpp`

修改 `src/app/topology/CMakeLists.txt`：

- **`TopologyEditorWidget` 继承自 `etest::app::IEditor`，需要看到 `editor/IEditor.h`**。`etest_topology` 库当前的包含路径仅 `src/app/topology/`。添加：
  ```cmake
  target_include_directories(etest_topology PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../)
  # 或改为基于 CMAKE_SOURCE_DIR 的绝对路径
  target_include_directories(etest_topology PUBLIC ${CMAKE_SOURCE_DIR}/src/app)
  ```

### 文件变更汇总

| 操作 | 文件 |
|---|---|
| **新建** | `src/app/editor/IEditor.h` |
| **新建** | `src/app/editor/EditorFactory.h` |
| **新建** | `src/app/editor/EditorFactory.cpp` |
| **改名** | `EditorWidget.h` → `TextEditorWidget.h` |
| **改名** | `EditorWidget.cpp` → `TextEditorWidget.cpp` |
| **修改** | `TextEditorWidget.h` — 改为 `: public QWidget, public IEditor` |
| **修改** | `TextEditorWidget.cpp` — 实现 IEditor 接口方法（`save()`→`saveFile()`, `saveAs()`→`saveFileAs()`, `editorId()`→`file_path_` 等） |
| **修改** | `TopologyEditorWidget.h` — 改为 `: public QWidget, public IEditor`，新增 `document()`, `reloadScene()`, 信号 `modificationChanged(bool)`, `editorIdChanged(QString,QString)` |
| **修改** | `TopologyEditorWidget.cpp` — 实现 IEditor 接口方法，`saveAs()` 成功后更新 editorId 并 emit `editorIdChanged` |
| **修改** | `TopologyDocument.h` — 新增 `modified_`, `setModified()`, `isModified()`，`addPort()`/`removePort()`/`deviceChanged()`/`productChanged()` 均设脏标记 |
| **修改** | `TopologyDocument.cpp` — 实现脏标记逻辑 |
| **修改** | `EditorManager.h` — `EditorWidget*` → `IEditor*`，新增 `updateEditorId()`, `createEditor()` |
| **修改** | `EditorManager.cpp` — 内部类型 + 工厂逻辑，`onFileDeleted()`/`onFileRenamed()` 加 `dynamic_cast`，编辑器 ID 生命周期管理 |
| **修改** | `MainWindow.cpp` — `currentEditorChanged` lambda 中 `EditorWidget*` → `IEditor*`，编辑操作加 `dynamic_cast` |
| **修改** | `main.cpp` — 注册工厂，拓扑工厂使用公开 API（`editor->document()`, `editor->reloadScene()`），含 JSON 解析错误处理 |
| **修改** | `src/app/CMakeLists.txt` — `EditorWidget` → `TextEditorWidget` + 新增 `editor/` 目录文件 |
| **修改** | `src/app/topology/CMakeLists.txt` — 为 `etest_topology` 库添加 `src/app/` 包含路径（`#include "editor/IEditor.h"`） |
